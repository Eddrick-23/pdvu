#pragma once
#include <memory>
#include <optional>
#include <string>

#include "mupdf_resources.h"
#include "page_specs.h"

namespace pdf {

/**
 * @brief A thread-safe handle for a cached sequence of MuPDF drawing commands.
 *
 * An fz_display_list stores a read-only, intermediate representation of a parsed
 * PDF page. Because it caches the drawing operations (paths, text, images) rather
 * than the raw page data, it allows multiple threads to concurrently render different
 * clipping regions or zoom levels of the same page without data races or re-parsing overhead.
 *
 * This alias uses a custom deleter to ensure the list is properly freed within the
 * correct fz_context when all references are dropped.
 */
using DisplayListHandle = std::shared_ptr<fz_display_list>;

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
   * @return A shared DisplayListHandle containing the cached draw commands.
   */
  virtual std::optional<DisplayListHandle> get_display_list(int page_num) = 0;

  /**
   * @brief Writes a specific clipped section of a display list to a buffer.
   *
   * Designed to be called by multiple threads concurrently. Each thread targets
   * a different clip and writes directly into the shared buffer offset.
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
   * @brief Clones the current parser state, creating a new context and loading the same document.
   * @return A unique pointer to the duplicated parser.
   */
  [[nodiscard]] virtual std::unique_ptr<Parser> duplicate() const = 0;
};

/**
 * @brief Concrete implementation of the Parser interface utilizing the MuPDF library.
 *
 * Manages the global MuPDF context (with thread-safe mutex locks) and document state.
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

  std::shared_ptr<MuPDFContext> context;  ///< RAII wrapper over core MuPDF context.
  fz_document* doc;                       ///< Pointer to the currently opened document
  std::string doc_name;                   ///< The filename of the document
  std::string full_filepath;              ///< Absolute path used for duplication
  bool use_icc_profile;                   ///< Flag indicating if ICC colour profiles are active
};
}  // namespace pdf
