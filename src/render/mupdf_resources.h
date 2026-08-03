#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

extern "C" {
#include <mupdf/fitz.h>
}

namespace pdf {

/**
 * @brief RAII wrapper over core mupdf context.
 *
 * This is meant to be used with std::shared_ptr or std::unique_ptr.
 * Use std::shared_ptr to ensure any exposed mupdf resources do not
 * outlive the core fz_context itself.
 */
class MuPDFContext final {
 public:
  /**
   * @param ctx A non null fz_context*
   * @throws std::invalid_argument if nullptr passed in
   */
  explicit MuPDFContext(fz_context* ctx) : m_ctx(ctx) {
    if (m_ctx == nullptr) {
      throw std::invalid_argument("Null MuPDF context");
    }
  }

  ~MuPDFContext() noexcept { fz_drop_context(m_ctx); }

  // no copy and no move
  MuPDFContext(const MuPDFContext&) = delete;
  MuPDFContext& operator=(const MuPDFContext&) = delete;
  MuPDFContext(MuPDFContext&&) = delete;
  MuPDFContext& operator=(MuPDFContext&&) = delete;

  /**
   * Returns a non-owning pointer to the managed MuPDF context.
   *
   * The caller must not call fz_drop_context() on this pointer or retain it
   * beyond the lifetime of this MuPDFContext.
   */
  [[nodiscard]] fz_context* borrow() const noexcept { return m_ctx; }

 private:
  fz_context* m_ctx;
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