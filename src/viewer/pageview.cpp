#include "pageview.h"

#include <algorithm>
#include <cmath>

void PageView::change_zoom_index(const int delta) {
  static constexpr int max_idx = zoom_levels.size() - 1;
  viewport.zoom_index = std::clamp(viewport.zoom_index + delta, 0, max_idx);
  viewport.zoom = zoom_levels[viewport.zoom_index];
}

PageView::CropRect PageView::calculate_crop_window(const int width, const int height,
                                                   const ViewportBounds& vb) const {
  // check if no crop needed
  if (width <= vb.max_width_pixels && height <= vb.max_height_pixels) {
    // no crop needed, whole image fits in window
    return CropRect{
        .x_offset_pixels = 0,
        .y_offset_pixels = 0,
        .width = width,
        .height = height,
    };
  }

  // calculate window
  const int x_offset_pixels = std::floor(static_cast<float>(width) * viewport.rel_x_offset);
  const int y_offset_pixels = std::floor(static_cast<float>(height) * viewport.rel_y_offset);
  const int crop_w = std::min(vb.max_width_pixels, width - x_offset_pixels);
  const int crop_h = std::min(vb.max_height_pixels, height - y_offset_pixels);
  return CropRect{
      .x_offset_pixels = x_offset_pixels,
      .y_offset_pixels = y_offset_pixels,
      .width = crop_w,
      .height = crop_h,
  };
}