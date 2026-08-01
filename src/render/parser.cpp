#include "parser.h"

#include <filesystem>
#include <mutex>
#include <print>

#include "page_specs.h"
#include "pdf_constants.h"
#include "plog/Log.h"
#include "utils/logging.h"
#include "utils/profiling.h"

namespace {
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

/**
 * @brief Creates a fresh MuPDF context with thread-safe locking configured.
 * @return A newly-created context, or nullptr if creation failed.
 */
fz_context* create_locked_context() {
  // FZ_STORE_DEFAULT = default resource cache size
  static fz_locks_context locks_context = make_locks_context();
  fz_context* new_ctx = fz_new_context(NULL, &locks_context, FZ_STORE_DEFAULT);
  if (new_ctx != nullptr) {
    fz_register_document_handlers(new_ctx);
  }
  return new_ctx;
}

}  // namespace

using namespace pdf;

MuPDFParser::MuPDFParser(const bool use_ICC, fz_context* cloned_ctx)
    : ctx(cloned_ctx != nullptr ? cloned_ctx : create_locked_context()),
      doc(nullptr),
      use_icc_profile(use_ICC) {
  if (ctx == nullptr) {
    throw std::runtime_error("Failed to create MuPDF context");
  }

  // Disable ICC colour management for performance
  if (!use_ICC) {
    fz_try(ctx) { fz_disable_icc(ctx); }
    fz_catch(ctx) { PLOG_WARNING << "Failed to configure ICC"; }
  }

  // fz_try(ctx) { // configuring anti-aliasing level
  //     // fz_set_aa_level(ctx, 0);
  //     fz_set_text_aa_level(ctx, 0);
  //     fz_set_graphics_aa_level(ctx, 0);
  // }
  // fz_catch(ctx) {
  //     std::cerr << "WARNING: Failed to configure AAC." << std::endl;
  // }
}

void MuPDFParser::clear_doc() {
  if (doc != nullptr) {
    fz_drop_document(ctx, doc);
    doc = nullptr;
  }
}

MuPDFParser::~MuPDFParser() {
  // Cleanup
  MuPDFParser::clear_doc();
  if (ctx != nullptr) {
    fz_drop_context(ctx);
    ctx = nullptr;
  }
}

bool MuPDFParser::load_document(const std::string& filepath) {
  clear_doc();
  fz_try(ctx) { doc = fz_open_document(ctx, filepath.c_str()); }
  fz_catch(ctx) {
    PLOG_ERROR << std::format("Could not open file: {}", filepath);
    return false;
  }
  full_filepath = filepath;  // save path for duplicating
  std::filesystem::path p(filepath);
  doc_name = p.filename().string();
  return true;
}

const std::string& MuPDFParser::get_document_name() const { return doc_name; }

int MuPDFParser::num_pages() const {
  if (doc == nullptr) {
    return 0;
  }
  int count = 0;
  fz_try(ctx) { count = fz_count_pages(ctx, doc); }
  fz_catch(ctx) {
    PLOG_ERROR << "Error: Failed to count pages.";
    return 0;
  }
  return count;
}

std::optional<PageSpecs> MuPDFParser::page_specs(int page_num) const {
  ZoneScoped;
  fz_page* page = nullptr;
  fz_try(ctx) { page = fz_load_page(ctx, doc, page_num); }
  fz_catch(ctx) {
    PLOG_ERROR << std::format("Error: Failed to load page. PageNum: {}", page_num);
    return {};
  }

  // create a scaling matrix to determine how much to scale the page
  // relative to its original base size
  fz_matrix ctm = fz_scale(g_base_zoom, g_base_zoom);

  // raw data
  fz_rect raw_bounds = fz_bound_page(ctx, page);
  raw_bounds = fz_transform_rect(raw_bounds, ctm);

  // round to int
  const fz_irect bbox = fz_round_rect(raw_bounds);
  fz_drop_page(ctx, page);

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
  fz_context* captured_ctx = this->ctx;  // capture for custom deleter
  return DisplayListHandle(raw_display_list, [captured_ctx](fz_display_list* ptr) {
    if (ptr) {
      fz_drop_display_list(captured_ctx, ptr);
    }
  });
}

void MuPDFParser::write_section(int w, int h, float zoom, const PageSpecs& ps,
                                DisplayListHandle dlist, unsigned char* buffer, Rect clip) {
  /* dlist is created by another thread. That thread will be responsible for
   * dropping it clip is which portion of the dlist we are reading from. it must
   * match with w and h The input buffer must be shifted such that the first
   * pixel drawn is at its correct position This allows multiple threads to
   * write to the buffer in parallel all to different sections at once
   */
  ZoneScoped;
  auto translate_matrix = [ps](fz_matrix& ctm) {
    const int rot = ps.rotation;
    const int total_w = ps.width;
    const int total_h = ps.height;
    if (rot == 90) {
      ctm = fz_concat(ctm, fz_translate(total_w, 0));
    } else if (rot == 180) {
      ctm = fz_concat(ctm, fz_translate(total_w, total_h));
    } else if (rot == 270) {
      ctm = fz_concat(ctm, fz_translate(0, total_h));
    }
  };
  if (w != clip.x1 - clip.x0 || h != clip.y1 - clip.y0) {
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
  fz_pixmap* pix = nullptr;
  fz_try(ctx) {
    fz_matrix ctm = fz_scale(zoom, zoom);
    ctm = fz_pre_rotate(ctm, ps.rotation);
    translate_matrix(ctm);
    pix = fz_new_pixmap_with_bbox_and_data(
        ctx, fz_device_rgb(ctx), fz_irect_from_rect(rect), NULL, 0, buffer);
    pix->x = static_cast<int>(clip.x0);
    pix->y = static_cast<int>(clip.y0);
    fz_clear_pixmap_with_value(ctx, pix, 255);  // set white background
    fz_device* dev = fz_new_draw_device(ctx, fz_identity, pix);
    fz_run_display_list(ctx, dlist.get(), dev, ctm, rect, NULL);

    // free memory
    fz_close_device(ctx, dev);
    fz_drop_device(ctx, dev);
    fz_drop_pixmap(ctx, pix);
  }
  fz_catch(ctx) {
    if (pix != nullptr) {
      fz_drop_pixmap(ctx, pix);
    }
    PLOG_ERROR << "Failed to draw page";
  }
}

std::unique_ptr<Parser> MuPDFParser::duplicate() const {
  fz_context* clone_ctx = fz_clone_context(ctx);
  auto new_parser =
      std::make_unique<MuPDFParser>(this->use_icc_profile, clone_ctx);  // debug try without clone

  if (!full_filepath.empty()) {
    if (!new_parser->load_document(this->full_filepath)) {
      throw std::runtime_error("Failed to load document for cloned parser");
    }
  }
  return new_parser;
}

MuPDFParser::MuPDFParser(MuPDFParser&& other) noexcept {
  ctx = other.ctx;
  doc = other.doc;
  doc_name = other.doc_name;
  use_icc_profile = other.use_icc_profile;

  other.ctx = nullptr;
  other.doc = nullptr;
  other.doc_name.clear();
  other.use_icc_profile = false;
}

MuPDFParser& MuPDFParser::operator=(MuPDFParser&& other) noexcept {
  if (this != &other) {
    // clear current processes
    clear_doc();
    if (ctx != nullptr) {
      fz_drop_context(ctx);
    }

    ctx = other.ctx;
    doc = other.doc;
    doc_name = other.doc_name;
    use_icc_profile = other.use_icc_profile;

    other.ctx = nullptr;
    other.doc = nullptr;
    other.doc_name.clear();
    other.use_icc_profile = false;
  }
  return *this;
}
