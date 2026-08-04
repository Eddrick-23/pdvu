#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

extern "C" {
#include <mupdf/fitz.h>
}

namespace pdf {

/**
 * @brief Exclusive RAII owner of a non-null MuPDF context.
 *
 * Internally stores the raw `fz_context` in a `UniqueHandle`, ensuring
 * `fz_drop_context()` is called exactly once when this wrapper is destroyed.
 *
 * Instances are normally managed through `std::shared_ptr` so dependent MuPDF
 * resources, such as display lists, can extend the context's lifetime.
 *
 * Sharing this wrapper controls lifetime only. It does not make simultaneous
 * operations on the same `fz_context` thread-safe; concurrent workers must use
 * appropriately cloned contexts.
 */
class MuPDFContext final {
 public:
  /**
   * @brief Stateless deleter used by UniqueHandle.
   *
   * Releases a non-null context with `fz_drop_context()`. Calling the deleter
   * with `nullptr` has no effect.
   */
  struct ContextDeleter {
    void operator()(fz_context* ctx) const noexcept {
      if (ctx != nullptr) {
        fz_drop_context(ctx);
      }
    }
  };

  /**
   * @brief Exclusive owning handle for a raw MuPDF context.
   *
   * Used while acquiring, initializing, or transferring a context before shared
   * ownership is published. Destruction releases the context automatically.
   */
  using UniqueHandle = std::unique_ptr<fz_context, ContextDeleter>;

  /**
   * @brief Constructs a wrapper by taking exclusive ownership of a context.
   *
   * @param ctx Non-null unique owner whose context is transferred into this
   * wrapper.
   * @throws std::invalid_argument If ctx is empty.
   */
  explicit MuPDFContext(UniqueHandle ctx) : m_ctx(std::move(ctx)) {
    if (m_ctx == nullptr) {
      throw std::invalid_argument("Null MuPDF context");
    }
  }

  ~MuPDFContext() noexcept = default;

  // no copy and no move
  MuPDFContext(const MuPDFContext&) = delete;
  MuPDFContext& operator=(const MuPDFContext&) = delete;
  MuPDFContext(MuPDFContext&&) = delete;
  MuPDFContext& operator=(MuPDFContext&&) = delete;

  /**
   * Returns a non-owning pointer.
   *
   * The caller must not call fz_drop_context() on this pointer or retain it
   * beyond the lifetime of this MuPDFContext.
   *
   * @return The managed, non-null `fz_context`.
   */
  [[nodiscard]] fz_context* borrow() const noexcept { return m_ctx.get(); }

 private:
  UniqueHandle m_ctx;
};

/**
 * @brief RAII wrapper over core mupdf display list.
 *
 * Meant to control lifetime of fz_display_list with its associated fz_context.
 * Construction requires std::shared_ptr<MuPDFContext> to be passed in. It should
 * hold the owning fz_context used to drop the display list.
 */
class MuPDFDisplayList final {
 public:
  /**
   * @param ctx A shared_ptr to a MuPDFContext containing a valid fz_context.
   * @param dlist A non null fz_display_list*.
   * @throws std::invalid_argument if nullptr passed in for either parameter.
   */
  explicit MuPDFDisplayList(std::shared_ptr<MuPDFContext> ctx, fz_display_list* dlist)
      : m_context(std::move(ctx)), m_dlist(dlist) {
    if (m_context == nullptr) {
      throw std::invalid_argument("Null MuPDF context wrapper");
    }
    if (m_dlist == nullptr) {
      throw std::invalid_argument("Null MuPDF display list");
    }
  }

  ~MuPDFDisplayList() noexcept { fz_drop_display_list(m_context->borrow(), m_dlist); }

  // no copy and no move
  MuPDFDisplayList(const MuPDFDisplayList&) = delete;
  MuPDFDisplayList& operator=(const MuPDFDisplayList&) = delete;
  MuPDFDisplayList(MuPDFDisplayList&&) = delete;
  MuPDFDisplayList& operator=(MuPDFDisplayList&&) = delete;

  /**
   * Returns a non-owning pointer to the managed MuPDF display list.
   *
   * The caller must not call fz_drop_display_list() on this pointer or retain it
   * beyond the lifetime of this MuPDFDisplayList.
   */
  [[nodiscard]] fz_display_list* borrow() const noexcept { return m_dlist; }

 private:
  std::shared_ptr<MuPDFContext> m_context;
  fz_display_list* m_dlist;
};
}  // namespace pdf