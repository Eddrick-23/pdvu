#include "parser.h"

#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <utility>

#include "page_specs.h"
#include "pdf_constants.h"
#include "plog/Log.h"
#include "utils/logging.h"
#include "utils/profiling.h"

namespace {

/**
 * @brief Internal facilities for creating and adopting MuPDF contexts.
 */
namespace mupdf_context_factory {

// -----------------------------------------------------------------------------
// MuPDF locking configuration
// -----------------------------------------------------------------------------

/**
 * @brief Wrapper struct for MuPDF's required threading mutexes.
 *
 * By default, MuPDF is not thread safe. To allow multiple threads to safely execute
 * concurrent operations(like parallel rendering), using the same underlying fz_context,
 * MuPDF requires an array of FZ_LOCK_MAX mutexes
 */
struct MutexLocks {
  std::array<std::mutex, FZ_LOCK_MAX> mutexes;
};

/**
 * @brief Global storage for mutexes used by the primary MuPDF context.
 *
 * This instance is passed as an opaque 'user' data pointer into
 * the fz_locks_context struct during the initial context creation
 */
MutexLocks global_mu_locks;

/**
 * @brief MuPDF callback invoked when the library needs to acquire a lock.
 *
 * @param user Opaque pointer to the user-provided lock state (our MutexLocks instance).
 * @param lock The internal ID of the mutex to lock (0 to FZ_LOCK_MAX - 1).
 */
void lock_callback(void* user, int lock) { static_cast<MutexLocks*>(user)->mutexes[lock].lock(); }

/**
 * @brief MuPDF callback invoked when the library needs to release a lock.
 *
 * @param user Opaque pointer to the user-provided lock state (our MutexLocks instance).
 * @param lock The internal ID of the mutex to unlock.
 */
void unlock_callback(void* user, int lock) {
  static_cast<MutexLocks*>(user)->mutexes[lock].unlock();
}

/**
 * @brief Configure an fz_locks_context to be used with cloned contexts
 *        to enable thread safe library operations.
 * @return a static fz_lock_context to be passed into fz_new_context()
 */
fz_locks_context make_locks_context() {
  static fz_locks_context locks_ctx{
      .user = &global_mu_locks,
      .lock = lock_callback,
      .unlock = unlock_callback,
  };
  return locks_ctx;
}

// -----------------------------------------------------------------------------
// Context ownership and publication
// -----------------------------------------------------------------------------

/**
 * @brief Exclusive ownership of a raw MuPDF context.
 *
 * A ContextOwner is used during context acquisition and initialization. If an
 * operation fails before shared ownership is established, its deleter releases
 * the context automatically.
 */
using ContextOwner = pdf::MuPDFContext::UniqueHandle;

/**
 * @brief Shared ownership of a fully initialized MuPDF context wrapper.
 *
 * Copies extend the lifetime of the same underlying context. Shared ownership
 * does not permit simultaneous MuPDF operations using that context.
 */
using SharedContext = std::shared_ptr<pdf::MuPDFContext>;

/**
 * @brief Publishes an exclusively owned context through shared ownership.
 *
 * Transfers `owner` into a newly allocated `MuPDFContext`. If allocation or
 * construction fails, either the local ContextOwner or the partially
 * constructed wrapper releases the context.
 *
 * @param owner Non-null exclusive owner to transfer.
 * @return A non-null shared owner of the initialized context.
 * @throws std::invalid_argument If `owner` is empty.
 * @throws std::bad_alloc If the wrapper or shared ownership control block
 * cannot be allocated.
 */
SharedContext publish_context(ContextOwner owner) {
  return std::make_shared<pdf::MuPDFContext>(std::move(owner));
}

/**
 * @brief Creates and initializes a MuPDF context with shared locking.
 *
 * Acquires a new raw context, immediately places it under exclusive ownership,
 * and registers MuPDF's document handlers. Shared ownership is published only
 * after initialization completes successfully.
 *
 * @return A non-null shared owner of the initialized context.
 * @throws std::runtime_error If MuPDF cannot allocate the context or register
 * its document handlers.
 * @throws std::bad_alloc If the C++ wrapper or shared ownership control block
 * cannot be allocated.
 *
 * @note The acquired context is released automatically on every failure path.
 */
SharedContext create_locked_context() {
  // FZ_STORE_DEFAULT = default resource cache size
  static fz_locks_context locks_context = make_locks_context();
  ContextOwner owner{fz_new_context(nullptr, &locks_context, FZ_STORE_DEFAULT)};

  if (owner == nullptr) {
    throw std::runtime_error("Failed to allocate MuPDF context");
  }
  fz_context* ctx = owner.get();
  fz_try(ctx) { fz_register_document_handlers(ctx); }
  fz_catch(ctx) {
    // since fz_context is wrapper with with a custom deleter
    // The deleter will cleanup resources for us.
    throw std::runtime_error("Failed to register MuPDF document handlers");
  }
  return publish_context(std::move(owner));
}

/**
 * @brief Adopts a raw cloned context and publishes shared ownership.
 *
 * Ownership of `raw_context` transfers to this function immediately. After
 * calling this function, the caller must not use or drop the raw pointer,
 * regardless of whether the function succeeds.
 *
 * @param raw_context Context returned by `fz_clone_context()`.
 * @return A non-null shared owner of the cloned context.
 * @throws std::runtime_error If `raw_context` is null.
 * @throws std::bad_alloc If the C++ wrapper or shared ownership control block
 * cannot be allocated.
 *
 * @note A non-null context is released automatically if publication fails.
 */
SharedContext adopt_context(fz_context* raw_context) {
  ContextOwner owner{raw_context};

  if (owner == nullptr) {
    throw std::runtime_error("Failed to clone MuPDF context");
  }

  return publish_context(std::move(owner));
}
}  // namespace mupdf_context_factory
}  // namespace

using namespace pdf;

MuPDFParser::MuPDFParser(bool use_ICC) try
    : context(mupdf_context_factory::create_locked_context()),
      doc(nullptr),
      use_icc_profile(use_ICC) {
  fz_context* ctx = context->borrow();

  // Disable ICC colour management for performance
  if (!use_ICC) {
    fz_try(ctx) { fz_disable_icc(ctx); }
    fz_catch(ctx) { PLOG_WARNING << "Failed to configure ICC"; }
  }
} catch (const std::invalid_argument&) {
  throw std::runtime_error("Failed to create MuPDF context");
}

MuPDFParser::MuPDFParser(const bool use_ICC, std::shared_ptr<MuPDFContext> cloned_ctx)
    : context(std::move(cloned_ctx)), doc(nullptr), use_icc_profile(use_ICC) {
  if (context == nullptr) {
    throw std::invalid_argument("Null MuPDF context");
  }

  // Disable ICC colour management for performance
  fz_context* ctx = context->borrow();

  if (!use_ICC) {
    fz_try(ctx) { fz_disable_icc(ctx); }
    fz_catch(ctx) { PLOG_WARNING << "Failed to configure ICC"; }
  }
}

void MuPDFParser::ensure_valid_context() const {
  if (context == nullptr) {
    throw std::runtime_error(
        "Parser underlying context is null, calling object may have been "
        "moved from");
  }
}

void MuPDFParser::clear_doc() {
  if (doc != nullptr) {
    fz_drop_document(context->borrow(), doc);
    doc = nullptr;
  }

  doc_name.clear();
  document_path.clear();
}

MuPDFParser::~MuPDFParser() { MuPDFParser::clear_doc(); }

bool MuPDFParser::load_document(const std::filesystem::path& filepath) {
  ensure_valid_context();
  clear_doc();
  if (filepath.empty()) {
    PLOG_ERROR << "Cannot load empty document path";
    return false;
  }

  // first resolve the filepath of given path. We store absolute, lexically normalised path(no .
  // or ..)
  std::error_code error;
  std::filesystem::path resolved_path = std::filesystem::absolute(filepath, error);

  if (error) {
    PLOG_ERROR << std::format(
        "Could not resolve document path '{}': {}", filepath.string(), error.message());
    return false;
  }

  resolved_path = resolved_path.lexically_normal();
  const std::string resolved_path_string = resolved_path.string();

  fz_context* ctx = context->borrow();
  fz_try(ctx) { doc = fz_open_document(ctx, resolved_path_string.c_str()); }
  fz_catch(ctx) {
    PLOG_ERROR << std::format("Could not open file: {}", resolved_path_string);
    return false;
  }
  document_path = resolved_path;  // save path for duplicating
  doc_name = document_path.filename().string();
  return true;
}

const std::string& MuPDFParser::get_document_name() const { return doc_name; }

int MuPDFParser::num_pages() const {
  ensure_valid_context();
  if (doc == nullptr) {
    return 0;
  }
  int count = 0;
  fz_context* ctx = context->borrow();
  fz_try(ctx) { count = fz_count_pages(ctx, doc); }
  fz_catch(ctx) {
    PLOG_ERROR << "Error: Failed to count pages.";
    return 0;
  }
  return count;
}

std::optional<PageSpecs> MuPDFParser::page_specs(int page_num) const {
  ZoneScoped;
  ensure_valid_context();
  if (doc == nullptr) {
    return std::nullopt;
  }
  fz_context* ctx = context->borrow();
  fz_page* page = nullptr;
  fz_rect raw_bounds{};
  fz_var(page);
  fz_try(ctx) {
    page = fz_load_page(ctx, doc, page_num);
    raw_bounds = fz_bound_page(ctx, page);
  }
  fz_always(ctx) { fz_drop_page(ctx, page); }
  fz_catch(ctx) {
    PLOG_ERROR << std::format("Failed to inspect page. PageNum: {}", page_num);
    return std::nullopt;
  }

  // create a scaling matrix to determine how much to scale the page
  // relative to its original base size
  const fz_matrix ctm = fz_scale(g_base_zoom, g_base_zoom);

  // raw page bounds with scaling applied
  raw_bounds = fz_transform_rect(raw_bounds, ctm);

  // round to int
  const fz_irect bbox = fz_round_rect(raw_bounds);

  // dimensions
  const int w = bbox.x1 - bbox.x0;
  const int h = bbox.y1 - bbox.y0;
  const size_t size = static_cast<size_t>(w) * g_pad * h;
  const float acc_height = raw_bounds.y1 - raw_bounds.y0;
  const float acc_width = raw_bounds.x1 - raw_bounds.x0;

  return PageSpecs{
      .base_x0 = raw_bounds.x0,
      .base_y0 = raw_bounds.y0,
      .base_x1 = raw_bounds.x1,
      .base_y1 = raw_bounds.y1,
      .x0 = bbox.x0,
      .y0 = bbox.y0,
      .x1 = bbox.x1,
      .y1 = bbox.y1,
      .width = w,
      .height = h,
      .size = size,
      .acc_width = acc_width,
      .acc_height = acc_height,
  };
}

std::optional<DisplayListHandle> MuPDFParser::get_display_list(int page_num) {
  ZoneScoped;
  ensure_valid_context();
  if (doc == nullptr) {
    return std::nullopt;
  }
  fz_context* ctx = context->borrow();
  fz_page* page = nullptr;
  fz_display_list* raw_display_list = nullptr;
  fz_try(ctx) { page = fz_load_page(ctx, doc, page_num); }
  fz_catch(ctx) {
    PLOG_ERROR << "MuPDFParser failed to load page";
    return std::nullopt;
  }
  fz_try(ctx) {
    raw_display_list = fz_new_display_list_from_page(ctx, page);
    fz_drop_page(ctx, page);
  }
  fz_catch(ctx) {
    if (page != nullptr) {
      fz_drop_page(ctx, page);
    }
    PLOG_ERROR << "MuPDFParser failed to create display list";
    return std::nullopt;
  }
  try {
    return std::make_shared<MuPDFDisplayList>(context, raw_display_list);
  } catch (const std::invalid_argument& e) {
    // In case internal invariants fail and null pointers passed to constructor.
    fz_drop_display_list(ctx, raw_display_list);
    PLOG_ERROR << "Invalid display list resources: " << e.what();
    return std::nullopt;
  } catch (...) {
    // any other unexpected error, clear resource and propagate
    fz_drop_display_list(ctx, raw_display_list);
    throw;
  }
}

void MuPDFParser::write_section(int w, int h, float zoom, const PageSpecs& ps,
                                DisplayListHandle dlist, unsigned char* buffer, Rect clip) {
  /* dlist is a wrapper over a fz_display_list. It will perform cleanup automatically
   * when no one else owns it.
   * clip is which portion of the dlist we are reading from. it must
   * match with w and h The input buffer must be shifted such that the first
   * pixel drawn is at its correct position This allows multiple threads to
   * write to the buffer in parallel all to different sections at once
   */
  ZoneScoped;
  ensure_valid_context();

  if (!dlist || buffer == nullptr) {
    PLOG_ERROR << "Cannot render with null display list or buffer";
    return;
  }

  auto translate_matrix = [ps](fz_matrix& ctm) {
    const int rot = ps.rotation;
    const auto total_w = static_cast<float>(ps.width);
    const auto total_h = static_cast<float>(ps.height);
    if (rot == 90) {
      ctm = fz_concat(ctm, fz_translate(total_w, 0));
    } else if (rot == 180) {
      ctm = fz_concat(ctm, fz_translate(total_w, total_h));
    } else if (rot == 270) {
      ctm = fz_concat(ctm, fz_translate(0, total_h));
    }
  };
  if (static_cast<float>(w) != clip.x1 - clip.x0 || static_cast<float>(h) != clip.y1 - clip.y0) {
    PLOG_ERROR << std::format(
        "clip dimensions do not match w:{} and "
        "h:{}. clip data: x0:{},y0:{},x1:{},y1:{}",
        w,
        h,
        clip.x0,
        clip.y0,
        clip.x1,
        clip.y1);
    return;
  }

  const auto rect = fz_rect(clip.x0, clip.y0, clip.x1, clip.y1);
  fz_context* ctx = context->borrow();
  fz_pixmap* pix = nullptr;
  fz_device* dev = nullptr;

  // required for locals modified inside fz_try
  fz_var(pix);
  fz_var(dev);

  fz_try(ctx) {
    fz_matrix ctm = fz_scale(zoom, zoom);
    ctm = fz_pre_rotate(ctm, static_cast<float>(ps.rotation));
    translate_matrix(ctm);
    pix = fz_new_pixmap_with_bbox_and_data(
        ctx, fz_device_rgb(ctx), fz_irect_from_rect(rect), nullptr, 0, buffer);
    pix->x = static_cast<int>(clip.x0);
    pix->y = static_cast<int>(clip.y0);
    fz_clear_pixmap_with_value(ctx, pix, 255);  // set white background
    dev = fz_new_draw_device(ctx, fz_identity, pix);
    fz_run_display_list(ctx, dlist->borrow(), dev, ctm, rect, nullptr);

    // close if nothing happened
    fz_close_device(ctx, dev);
  }
  fz_always(ctx) {
    if (dev != nullptr) {
      fz_drop_device(ctx, dev);
    }
    if (pix != nullptr) {
      fz_drop_pixmap(ctx, pix);
    }
  }
  fz_catch(ctx) { PLOG_ERROR << "Failed to draw page"; }
}

std::unique_ptr<Parser> MuPDFParser::duplicate() const {
  ensure_valid_context();

  auto cloned_ctx = mupdf_context_factory::adopt_context(fz_clone_context(context->borrow()));

  auto new_parser =
      std::unique_ptr<MuPDFParser>(new MuPDFParser(this->use_icc_profile, std::move(cloned_ctx)));

  if (!document_path.empty()) {
    if (!new_parser->load_document(this->document_path)) {
      throw std::runtime_error("Failed to load document for cloned parser");
    }
  }
  return new_parser;
}

MuPDFParser::MuPDFParser(MuPDFParser&& other) noexcept
    : context(std::move(other.context)),
      doc(std::exchange(other.doc, nullptr)),
      doc_name(std::move(other.doc_name)),
      document_path(std::move(other.document_path)),
      use_icc_profile(std::exchange(other.use_icc_profile, false)) {}

MuPDFParser& MuPDFParser::operator=(MuPDFParser&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  clear_doc();  // drop old document using current context first
  context = std::move(other.context);
  doc = std::exchange(other.doc, nullptr);
  doc_name = std::move(other.doc_name);
  document_path = std::move(other.document_path);
  use_icc_profile = std::exchange(other.use_icc_profile, false);

  return *this;
}
