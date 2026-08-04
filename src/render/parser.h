#pragma once
#include <memory>
#include <optional>
#include <string>

#include "mupdf_resources.h"
#include "page_specs.h"

namespace pdf {

/**
 * @brief Shared owning handle to an immutable MuPDF display list.
 *
 * An fz_display_list contains a cached sequence of drawing commands for a
 * parsed page. The display list can be reused to render the page at different
 * zoom levels or clipping regions without parsing the page again.
 *
 * The MuPDFDisplayList wrapper owns the underlying fz_display_list and retains
 * shared ownership of the MuPDFContext required to destroy it. Consequently,
 * the display list and its originating context remain alive until the final
 * DisplayListHandle is released.
 *
 * Handles may be copied across threads to keep the display list alive. Each rendering thread must
 * use its own cloned MuPDF context, and the owner must retain a coordinating handle until all
 * concurrent operations finish.
 */
using DisplayListHandle = std::shared_ptr<MuPDFDisplayList>;

/**
 * @brief Abstract interface defining the core operations of a PDF parser.
 */
struct Parser {
  /**
   * @brief Concrete implementations should override destructor for resource cleanup
   */
  virtual ~Parser() = default;
  /**
   * @brief Clears the currently loaded document and resets underlying tracked
   * document name and filepath
   */
  virtual void clear_doc() = 0;

  /**
   * @brief Loads a PDF document from the filesystem.
   *
   * Clears any currently loaded document before attempting the load
   * A failed load leaves the parser unloaded.
   *
   * @param filepath The path to the PDF file.
   * @return True if loaded successfully, false otherwise.
   */
  virtual bool load_document(const std::string& filepath) = 0;

  /** @return The filename of the currently loaded document. */
  [[nodiscard]] virtual const std::string& get_document_name() const = 0;

  /**
   * @brief Retrieves the dimensional specifications for a given page.
   * @param page_num The 0-indexed page number.
   * @return std::optional containing PageSpecs if successful.
   */
  [[nodiscard]] virtual std::optional<PageSpecs> page_specs(int page_num) const = 0;

  /** @return The total number of pages in the loaded document. */
  [[nodiscard]] virtual int num_pages() const = 0;

  /**
   * @brief Parses a PDF page and caches its rendering commands into a display list.
   *
   * This method performs the heavy lifting of interpreting the raw PDF page data once.
   * The resulting display list acts as a lightweight, thread-safe blueprint that can
   * be passed to concurrent workers for rapid, parallel rendering of distinct page segments.
   *
   * @param page_num The 0-indexed page number to parse.
   * @return std::nullopt when no document is loaded, the page index is invalid, or MuPDF cannot
   * construct the display list. An engaged optional always contains a non-null handle.
   */
  [[nodiscard]] virtual std::optional<DisplayListHandle> get_display_list(int page_num) = 0;

  /**
   * @brief Writes a specific clipped section of a display list to a buffer.
   *
   * Performs synchronous, best-effort rendering. Invalid inputs and MuPDF rendering failures are
   * logged and do not throw; the destination buffer may be partially written. The caller must
   * provide at least w * h * g_pad writable bytes.
   *
   * @param w The width of the clip.
   * @param h The height of the clip.
   * @param zoom The zoom scale factor.
   * @param ps The dimensions and rotation state of the page.
   * @param dlist The pre-computed display list to execute.
   * @param buffer Pointer to the start of the target memory buffer.
   * @param clip The exact fz_rect sub-region to render.
   */
  virtual void write_section(int w, int h, float zoom, const PageSpecs& ps, DisplayListHandle dlist,
                             unsigned char* buffer, Rect clip) = 0;

  /**
   * @brief Clones the MuPDF context and reopens the currently loaded document.
   * @return A unique pointer to the duplicated parser.
   * @throw std::runtime_error if context cloning or document reopening fails.
   */
  [[nodiscard]] virtual std::unique_ptr<Parser> duplicate() const = 0;
};

/**
 * @brief Concrete implementation of the Parser interface utilizing the MuPDF library.
 *
 * Owns a MuPDF context (configured with thread-safe mutex locks)
 */
class MuPDFParser : public Parser {
 public:
  /**
   * @brief Constructs the parser, initializing or cloning the MuPDF context.
   *        Creates a new underlying context with global locks.
   * @param use_ICC Whether to enable ICC color management.
   * @throws std::runtime_error if context creation fails.
   */
  explicit MuPDFParser(bool use_ICC);
  ~MuPDFParser() override;

  // Parser instances are non-copyable.
  MuPDFParser(const MuPDFParser&) = delete;
  MuPDFParser& operator=(const MuPDFParser&) = delete;

  // move semantics
  MuPDFParser(MuPDFParser&& other) noexcept;
  MuPDFParser& operator=(MuPDFParser&& other) noexcept;

  void clear_doc() override;
  bool load_document(const std::string& filepath) override;
  [[nodiscard]] const std::string& get_document_name() const override;
  [[nodiscard]] std::optional<PageSpecs> page_specs(int page_num) const override;
  [[nodiscard]] int num_pages() const override;
  [[nodiscard]] std::optional<DisplayListHandle> get_display_list(int page_num) override;
  void write_section(int w, int h, float zoom, const PageSpecs& ps, DisplayListHandle dlist,
                     unsigned char* buffer, Rect clip) override;
  [[nodiscard]] std::unique_ptr<Parser> duplicate() const override;

 private:
  /**
   * @brief Constructs the parser, initializing or cloning the MuPDF context.
   * @param use_ICC Whether to enable ICC color management.
   * @param cloned_ctx A cloned context from fz_clone_context
   * @throws std::invalid_argument if a null cloned_context is given
   */
  explicit MuPDFParser(bool use_ICC, std::shared_ptr<MuPDFContext> cloned_ctx);

  /**
   * @brief checks if context is still valid (not nullptr)
   * @throws std::runtime_error if context is invalid (e.g. calling parser methods after
   * it is moved).
   */
  void ensure_valid_context() const;

  std::shared_ptr<MuPDFContext> context;  ///< RAII wrapper over core MuPDF context.
  fz_document* doc;                       ///< Pointer to the currently opened document
  std::string doc_name;                   ///< The filename of the document
  std::string full_filepath;              ///< Absolute path used for duplication
  bool use_icc_profile;                   ///< Flag indicating if ICC colour profiles are active
};
}  // namespace pdf
