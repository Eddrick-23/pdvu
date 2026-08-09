#include "frame_layout.h"

#include <algorithm>

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

  // scale crop window to fit existing bitmap if dimensions mismatch
  if (source.width != target.width || source.height != target.height) {
    const float scale_factor_x =
        static_cast<float>(target.width) / static_cast<float>(source.width);
    const float scale_factor_y =
        static_cast<float>(target.height) / static_cast<float>(source.height);
    result.source_crop_rect.x =
        static_cast<int>(static_cast<float>(target_crop.x) / scale_factor_x);
    result.source_crop_rect.y =
        static_cast<int>(static_cast<float>(target_crop.y) / scale_factor_y);
    result.source_crop_rect.width =
        static_cast<int>(static_cast<float>(target_crop.width) / scale_factor_x);
    result.source_crop_rect.height =
        static_cast<int>(static_cast<float>(target_crop.height) / scale_factor_y);
    result.source_matches_target = false;
  }

  return result;
}
}  // namespace viewer