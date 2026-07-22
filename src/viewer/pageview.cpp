#include "pageview.h"

#include <algorithm>
#include <cmath>

void PageView::change_zoom_index(const int delta) {
  static constexpr int max_idx = zoom_levels.size() - 1;
  viewport.zoom_index = std::clamp(viewport.zoom_index + delta, 0, max_idx);
  viewport.zoom = zoom_levels[viewport.zoom_index];
}

PageView::CropRect PageView::calculate_crop_window(
    const int width, const int height, const ViewportBounds& vBounds) const {
  // check if no crop needed, whole image fits in window
  if (width <= vBounds.max_width_pixels && height <= vBounds.max_height_pixels) {
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
  const int crop_w = std::min(vBounds.max_width_pixels, width - x_offset_pixels);
  const int crop_h = std::min(vBounds.max_height_pixels, height - y_offset_pixels);
  return CropRect{
      .x_offset_pixels = x_offset_pixels,
      .y_offset_pixels = y_offset_pixels,
      .width = crop_w,
      .height = crop_h,
  };
}

void PageView::update_viewport(const ViewportBounds& vBounds, const ImageDimensions& imgDim,
    const float delta_x, const float delta_y) {
  // calculate size of window relative to image
  const float view_ratio_w =
      static_cast<float>(vBounds.max_width_pixels) / static_cast<float>(imgDim.width);
  const float view_ratio_h =
      static_cast<float>(vBounds.max_height_pixels) / static_cast<float>(imgDim.height);

  // calculate max offsets
  const float max_x_offset = std::max(0.0F, 1.0F - view_ratio_w);
  const float max_y_offset = std::max(0.0F, 1.0F - view_ratio_h);

  // update our viewport offsets
  viewport.rel_x_offset = std::clamp(viewport.rel_x_offset + delta_x, 0.0F, max_x_offset);
  viewport.rel_y_offset = std::clamp(viewport.rel_y_offset + delta_y, 0.0F, max_y_offset);
}