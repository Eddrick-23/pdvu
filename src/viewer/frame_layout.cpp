#include "frame_layout.h"

#include <algorithm>
#include <cmath>

namespace viewer {
FrameLayout calculate_frame_layout(geometry::PixelSize source, geometry::PixelSize target,
                                   geometry::PixelRect target_crop, const TermSize& ts,
                                   const TUI::ContentArea& content_area) {
  auto round_to_nearest_cell = [](int pixels, int pixels_per_cell, int max_cells) {
    return std::clamp(static_cast<int>(std::lround(static_cast<double>(pixels) /
                                                   static_cast<double>(pixels_per_cell))),
                      1,
                      max_cells);
  };
  const int placement_cols =
      round_to_nearest_cell(target_crop.width, ts.cell_pixel_width, content_area.cols);
  const int placement_rows =
      round_to_nearest_cell(target_crop.height, ts.cell_pixel_height, content_area.rows);

  auto [start_row, start_col] =
      TUI::centered_cursor_position(ts, target.width, target.height, content_area);
  FrameLayout result{
      .target_crop_rect = target_crop,
      .source_crop_rect = target_crop,
      .placement_origin = {.row = start_row, .col = start_col},
      .placement_cols = placement_cols,
      .placement_rows = placement_rows,
      .source_matches_target = true,
  };
  auto map_floor = [](int value, int source_size, int target_size) {
    return static_cast<int>(static_cast<std::int64_t>(value) * source_size / target_size);
  };

  auto map_ceil = [](int value, int source_size, int target_size) {
    const auto numerator = static_cast<std::int64_t>(value) * source_size;

    return static_cast<int>((numerator + target_size - 1) / target_size);
  };

  // scale crop window to fit existing bitmap if dimensions mismatch
  if (source.width != target.width || source.height != target.height) {
    // lower bounds are rounded down, upper bounds are rounded up.
    const int source_x0 = map_floor(target_crop.x, source.width, target.width);
    const int source_y0 = map_floor(target_crop.y, source.height, target.height);
    const int source_x1 = map_ceil(target_crop.x + target_crop.width, source.width, target.width);
    const int source_y1 =
        map_ceil(target_crop.y + target_crop.height, source.height, target.height);
    result.source_matches_target = false;
    result.source_crop_rect = {
        .x = source_x0,
        .y = source_y0,
        .width = source_x1 - source_x0,
        .height = source_y1 - source_y0,
    };
  }

  return result;
}
}  // namespace viewer