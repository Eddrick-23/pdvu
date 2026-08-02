#pragma once

#include <stdexcept>

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
    if (ctx == nullptr) {
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
}  // namespace pdf