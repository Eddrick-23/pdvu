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
using DisplayListHandle = std::shared_ptr<fz_display_list>;

struct PageSpecs {
  float base_x0, base_y0, base_x1, base_y1; // store for manual scaling
  int x0, y0, x1, y1;
  int width, height;
  size_t size;
  float acc_width, acc_height;
  int rotation;

  PageSpecs scale(float new_zoom) const {
    // manual scaling without calling fz_bound_page again
    float x0 = base_x0 * new_zoom;
    float y0 = base_y0 * new_zoom;
    float x1 = base_x1 * new_zoom;
    float y1 = base_y1 * new_zoom;

    fz_irect rect = fz_round_rect({x0, y0, x1, y1});
    const int w = rect.x1 - rect.x0;
    const int h = rect.y1 - rect.y0;
    const size_t size = w * 3 * h;

    return PageSpecs(x0, y0, x1, y1, rect.x0, rect.y0, rect.x1, rect.y1, w, h,
                     size, x1 - x0, y1 - y0, rotation);
  }

  PageSpecs rotate_quarter_clockwise(int n) const {
    // Note since we never render a cropped image, we can simply take PageSpecs
    // as dimensions of the image. top left always starts at (0,0), bot right at
    // (0 + width, 0 + height)
    int new_rotation = (rotation + n * 90) % 360;
    new_rotation = new_rotation < 0 ? new_rotation + 360 : new_rotation;
    if (n == 1 || n == 3) {
      return PageSpecs(base_y0, base_x0, base_y1, base_x1, y0, x0, y1, x1,
                       height, width, size, acc_height, acc_width,
                       new_rotation);
    }
    PageSpecs new_specs = *this;
    new_specs.rotation = new_rotation;
    return new_specs;
  }
};

struct HorizontalBound {
  fz_rect rect;
  int width;
  int height;
  size_t bytes;
  size_t offset; // offset from start pointer of buffer
};
struct Parser {

  virtual ~Parser() = default;
  virtual void clear_doc() = 0;
  virtual bool load_document(const std::string &filepath) = 0;
  virtual const std::string &get_document_name() const = 0;
  virtual std::optional<PageSpecs> page_specs(int page) const = 0;
  virtual std::vector<HorizontalBound> split_bounds(PageSpecs, int n) = 0;
  virtual int num_pages() const = 0;
  virtual DisplayListHandle get_display_list(int page_num) = 0;
  virtual void write_section(int w, int h, float zoom, const PageSpecs& ps,
                             DisplayListHandle dlist, unsigned char *buffer,
                             fz_rect clip) = 0;
  virtual std::unique_ptr<Parser> duplicate() const = 0;
};

class MuPDFParser : public Parser {
public:
  explicit MuPDFParser(bool use_ICC, fz_context *cloned_ctx = nullptr);
  ~MuPDFParser() override;

  // delete copy constructors
  MuPDFParser(const MuPDFParser &) = delete;
  MuPDFParser &operator=(const MuPDFParser &) = delete;

  // move constructors
  MuPDFParser(MuPDFParser &&other) noexcept;
  MuPDFParser &operator=(MuPDFParser &&other) noexcept;

  void clear_doc() override;
  bool load_document(const std::string &filepath) override;
  const std::string &get_document_name() const override;
  [[nodiscard]] std::optional<PageSpecs> page_specs(int page) const override;
  [[nodiscard]] std::vector<HorizontalBound> split_bounds(PageSpecs ps,
                                                          int n) override;
  int num_pages() const override;
  [[nodiscard]] DisplayListHandle get_display_list(int page_num) override;
  void write_section(int w, int h, float zoom, const PageSpecs& ps,
                     DisplayListHandle dlist, unsigned char *buffer,
                     fz_rect clip) override;
  [[nodiscard]] std::unique_ptr<Parser> duplicate() const override;

private:
  fz_context *ctx;
  fz_document *doc;
  std::string doc_name;
  std::string full_filepath;
  bool use_icc_profile;
};
} // namespace pdf
