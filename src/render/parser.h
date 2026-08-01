#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

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
 * @brief Contains dimensions and bounding box specifications for a PDF page.
 *
 * Stores both the base dimensions and the integer-rounded bounding boxes
 * required for rendering and transformations.
 */
struct PageSpecs {
  float base_x0, base_y0, base_x1, base_y1;  ///< Base unscaled coordinate bounds
  int x0, y0, x1, y1;                        ///< Integer-rounded coordinate bounds
  int width, height;                         ///< Pixel dimensions of the page
  size_t size;                               ///< Total size in bytes required for RGB pixel data
  float acc_width, acc_height;               ///< Accurate unrounded floating-point dimensions
  int rotation;                              ///< Current rotation of the page in degrees

  /**
   * @brief Scales the page specifications manually without reloading the page bounds.
   *
   * @param new_zoom The scale multiplier to apply
   * @return A new PageSpecs instance with scaled dimensions
   */
  [[nodiscard]] PageSpecs scale(float new_zoom) const {
    // performs manual scaling without calling fz_bound_page again
    const float new_x0 = base_x0 * new_zoom;
    const float new_y0 = base_y0 * new_zoom;
    const float new_x1 = base_x1 * new_zoom;
    const float new_y1 = base_y1 * new_zoom;

    const fz_irect new_rect =
        fz_round_rect({.x0 = new_x0, .y0 = new_y0, .x1 = new_x1, .y1 = new_y1});
    const int new_width = std::abs(new_rect.x1 - new_rect.x0);
    const int new_height = std::abs(new_rect.y1 - new_rect.y0);
    const size_t new_size = static_cast<size_t>(new_width) * 3 * static_cast<size_t>(new_height);

    return PageSpecs{
        .base_x0 = base_x0,  // Preserve original unscaled base
        .base_y0 = base_y0,
        .base_x1 = base_x1,
        .base_y1 = base_y1,
        .x0 = new_rect.x0,
        .y0 = new_rect.y0,
        .x1 = new_rect.x1,
        .y1 = new_rect.y1,
        .width = new_width,
        .height = new_height,
        .size = new_size,
        .acc_width = new_x1 - new_x0,
        .acc_height = new_y1 - new_y0,
        .rotation = rotation,
    };
  }

  /**
   * @brief Rotates the page dimensions in 90-degree clockwise increments.
   *
   * Flips width/height dimensions for 90 and 270-degree rotations while
   * preserving the top-left (0,0) origin constraint.
   *
   * @param n The number of 90-degree quarter turns to apply.
   * @return A new rotated PageSpecs instance.
   */
  [[nodiscard]] PageSpecs rotate_quarter_clockwise(int n) const {
    // Note since we never render a cropped image, we can simply take
    // PageSpecs as dimensions of the image. top left always starts at
    // (0,0), bot right at (0 + width, 0 + height)
    const bool flipped = std::abs(n) % 2 != 0;
    int new_rotation = (rotation + (n * 90)) % 360;
    new_rotation = new_rotation < 0 ? new_rotation + 360 : new_rotation;
    if (flipped) {
      return PageSpecs{
          .base_x0 = base_y0,
          .base_y0 = base_x0,
          .base_x1 = base_y1,
          .base_y1 = base_x1,
          .x0 = y0,
          .y0 = x0,
          .x1 = y1,
          .y1 = x1,
          .width = height,
          .height = width,
          .size = size,
          .acc_width = acc_height,
          .acc_height = acc_width,
          .rotation = new_rotation,

      };
    }
    PageSpecs new_specs = *this;
    new_specs.rotation = new_rotation;
    return new_specs;
  }
};

/**
 * @brief Represents a horizontal slice of a page for parallel processing.
 */
struct HorizontalBound {
  fz_rect rect;   ///< The MuPDF rectangle defining the physical bounds of this strip.
  int width;      ///< Pixel width of the strip.
  int height;     ///< Pixel height of the strip.
  size_t bytes;   ///< Total bytes contained in this strip.
  size_t offset;  ///< Memory offset in bytes from the start of the primary buffer.
};

/**
 * @brief Abstract interface defining the core operations of a PDF parser.
 */
struct Parser {
  /**
   * @brief Concrete implementations should override destructor for resource cleanup
   */
  virtual ~Parser() = default;
  /**
   * @brief Clears the currently loaded document and frees resources
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
   * @param page The 0-indexed page number.
   * @return std::optional containing PageSpecs if successful.
   */
  [[nodiscard]] virtual std::optional<PageSpecs> page_specs(int page) const = 0;

  /**
   * @brief Partitions a page into n horizontal strips for parallel rendering.
   * @param ps The PageSpecs of the page to split.
   * @param n The number of horizontal strips to generate.
   * @return A vector of HorizontalBound definitions.
   */
  [[nodiscard]] virtual std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n) = 0;

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
  virtual DisplayListHandle get_display_list(int page_num) = 0;

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
      unsigned char* buffer, fz_rect clip) = 0;

  /**
   * @brief Clones the current parser state, creating a new context and loading the same document.
   * @return A unique pointer to the duplicated parser[cite: 6].
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
   * @param use_ICC Whether to enable ICC color management.
   * @param cloned_ctx An optional existing context to clone for multi-threading. If null, a new
   * context with global locks is created.
   */
  explicit MuPDFParser(bool use_ICC, fz_context* cloned_ctx = nullptr);
  ~MuPDFParser() override;

  // Enforce unique ownership over mupdf context
  MuPDFParser(const MuPDFParser&) = delete;
  MuPDFParser& operator=(const MuPDFParser&) = delete;

  // move semantics
  MuPDFParser(MuPDFParser&& other) noexcept;
  MuPDFParser& operator=(MuPDFParser&& other) noexcept;

  void clear_doc() override;
  bool load_document(const std::string& filepath) override;
  [[nodiscard]] const std::string& get_document_name() const override;
  [[nodiscard]] std::optional<PageSpecs> page_specs(int page) const override;
  [[nodiscard]] std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n) override;
  [[nodiscard]] int num_pages() const override;
  [[nodiscard]] DisplayListHandle get_display_list(int page_num) override;
  void write_section(int w, int h, float zoom, const PageSpecs& ps, DisplayListHandle dlist,
      unsigned char* buffer, fz_rect clip) override;
  [[nodiscard]] std::unique_ptr<Parser> duplicate() const override;

 private:
  fz_context* ctx;            ///< The core MuPDF context required for all operations
  fz_document* doc;           ///< Pointer to the currently opened document
  std::string doc_name;       ///< The filename of the document
  std::string full_filepath;  ///< Absolute path used for duplication
  bool use_icc_profile;       ///< Flag indicating if ICC colour profiles are active
};
}  // namespace pdf
