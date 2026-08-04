#include "pageview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

bool PageView::change_zoom_index(const int delta) {
  static constexpr int max_idx = m_zoom_levels.size() - 1;
  const int old_zoom_index = m_viewport.zoom_index;
  m_viewport.zoom_index = std::clamp(m_viewport.zoom_index + delta, 0, max_idx);
  return old_zoom_index != m_viewport.zoom_index;
}

PageView::CropRect PageView::calculate_crop_window(const int width, const int height,
                                                   const ViewportBounds& vBounds) const {
  auto calculate_offset = [](int max_offset_pixels, float rel_offset) {
    return static_cast<int>(std::floor(static_cast<float>(max_offset_pixels) * rel_offset));
  };

  // calculate window
  const int max_x_offset_pixels = std::max(0, width - vBounds.max_width_pixels);
  const int max_y_offset_pixels = std::max(0, height - vBounds.max_height_pixels);
  const int x_offset_pixels = calculate_offset(max_x_offset_pixels, m_viewport.rel_x_offset);
  const int y_offset_pixels = calculate_offset(max_y_offset_pixels, m_viewport.rel_y_offset);
  const int crop_w = std::min(vBounds.max_width_pixels, width - x_offset_pixels);
  const int crop_h = std::min(vBounds.max_height_pixels, height - y_offset_pixels);
  return CropRect{
      .x_offset_pixels = x_offset_pixels,
      .y_offset_pixels = y_offset_pixels,
      .width = crop_w,
      .height = crop_h,
  };
}

bool PageView::update_viewport(const float delta_x, const float delta_y) {
  const float old_x_offset = m_viewport.rel_x_offset;
  const float old_y_offset = m_viewport.rel_y_offset;
  m_viewport.rel_x_offset = std::clamp(m_viewport.rel_x_offset + delta_x, 0.0F, 1.0F);
  m_viewport.rel_y_offset = std::clamp(m_viewport.rel_y_offset + delta_y, 0.0F, 1.0F);

  // check if any offset changed
  return old_x_offset != m_viewport.rel_x_offset || old_y_offset != m_viewport.rel_y_offset;
}

float PageView::current_zoom() const {
  return m_zoom_levels[static_cast<std::size_t>(m_viewport.zoom_index)];
}

void PageView::reset_zoom_to_default() { m_viewport.zoom_index = m_default_zoom_index; }

void PageView::reset_offsets_to_default() {
  m_viewport.rel_x_offset = 0.5;
  m_viewport.rel_y_offset = 0.5;
}