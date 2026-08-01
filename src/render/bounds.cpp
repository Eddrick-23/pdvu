
#include <vector>

#include "parser.h"
#include "pdf_constants.h"
#include "utils/profiling.h"

namespace pdf {
std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n) {
  ZoneScoped;
  // split the page into n horizontal strips represented by fz_rect
  // The height is divided evenly, while the remainder is tracked to ensure
  // no pixels are dropped at the bottom of the page due to integer division.
  std::vector<HorizontalBound> bounds;
  const int y_step = ps.height / n;
  const int remainder = ps.height % n;

  // horizontal bounds remain constant for every strip
  const int x0 = ps.x0;
  const int x1 = ps.x1;

  // Init vertical window for first horizontal strip(from the top)
  int y0 = ps.y0;
  int y1 = ps.y0 + y_step;
  size_t offset = 0;
  for (int i = 0; i < n - 1; i++) {
    const size_t pixels = static_cast<size_t>(x1 - x0) * static_cast<size_t>(y1 - y0) * g_pad;
    auto data = HorizontalBound{
        .rect =
            Rect{
                .x0 = static_cast<float>(x0),
                .y0 = static_cast<float>(y0),
                .x1 = static_cast<float>(x1),
                .y1 = static_cast<float>(y1),
            },
        .width = x1 - x0,
        .height = y1 - y0,
        .bytes = pixels,
        .offset = offset,
    };
    bounds.push_back(data);
    // slide vertical boundary down
    y0 = y1;
    y1 += y_step;
    // Advance buffer offset by exact byte size of current strip
    offset += pixels;
  }

  // last iteration: expand final strip's lower boundary by remaining pixels
  y1 += remainder;
  const size_t pixels = static_cast<size_t>(x1 - x0) * static_cast<size_t>(y1 - y0) * g_pad;
  const auto data = HorizontalBound{
      .rect =
          Rect{
              .x0 = static_cast<float>(x0),
              .y0 = static_cast<float>(y0),
              .x1 = static_cast<float>(x1),
              .y1 = static_cast<float>(y1),
          },
      .width = x1 - x0,
      .height = y1 - y0,
      .bytes = pixels,
      .offset = offset,
  };
  bounds.push_back(data);
  return bounds;
}
}  // namespace pdf