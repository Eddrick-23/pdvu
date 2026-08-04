#include "bounds.h"

#include <algorithm>
#include <format>
#include <vector>

#include "page_specs.h"
#include "pdf_constants.h"
#include "plog/Log.h"
#include "utils/profiling.h"

namespace pdf {
std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n) {
  ZoneScoped;
  // split the page into n horizontal strips represented by fz_rect
  // The height is divided evenly, while the remainder is tracked to ensure
  // no pixels are dropped at the bottom of the page due to integer division.

  if (n <= 0) {
    PLOG_ERROR << std::format("split bounds called with invalie n={}, clamping to 1", n);
    n = 1;
  }

  if (ps.height < 0) {
    PLOG_ERROR << std::format("split_bounds called with negative height={}, clamping to 0",
                              ps.height);
    ps.height = 0;
  }

  // can only produce as many strips as pixel rows
  n = std::min(n, std::max(ps.height, 1));

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
  for (int i = 0; i < n; i++) {
    if (i == n - 1) {
      y1 += remainder;
    }
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
  return bounds;
}
}  // namespace pdf