#pragma once

#include <array>

class PageView {
 public:
  /**
   * @brief Defines the maximum pixel area available for the view.
   * This should be pre-calculated by the caller to account for UI margins, bars, or offsets.
   */
  struct ViewportBounds {
    int max_height_pixels;
    int max_width_pixels;
  };

  /**
   * @brief Defines the crop rectangle to crop an image.
   *
   * The origin (0, 0) is taken to be the top-left corner of the image.
   */
  struct CropRect {
    int x_offset_pixels;  ///< Horizontal offset from the left edge.
    int y_offset_pixels;  ///< Vertical offset from the top edge.
    int width;            ///< The final width of the cropped area in pixels.
    int height;           ///< The final height of the cropped area in pixels.
  };

  PageView() = default;

  /**
   * @brief Shifts the current zoom level index up or down.
   *
   * @param delta How many indexes to shift by (e.g., 1 to zoom in, -1 to zoom out).
   */
  void change_zoom_index(int delta);

  /**
   * @brief Calculates the cropping rectangle to fit an image within the allowed viewport bounds.
   *
   * @param width The original width of the image in pixels.
   * @param height The original height of the image in pixels.
   * @param vb The maximum available pixel boundaries this view is allowed to draw in.
   * @return CropRect containing the pixel offsets and final dimensions to crop.
   */
  CropRect calculate_crop_window(int width, int height, const ViewportBounds& vb) const;

  void update_viewport(float delta_x, float delta_y);
  void handle_page_pan(char key);

 private:
  struct Viewport {
    float rel_x_offset;
    float rel_y_offset;
    int zoom_index;
    float zoom;
  };

  static constexpr std::array<float, 11> zoom_levels{0.5F, 0.67F, 0.75F, 0.8F,  0.9F, 1.0F,
                                                     1.1F, 1.25F, 1.5F,  1.75F, 2.0F};
  static constexpr int default_zoom_index = 5;  // 1.0x (100%)
  Viewport viewport{
      .rel_x_offset = 0.0,
      .rel_y_offset = 0.0,
      .zoom_index = default_zoom_index,
      .zoom = zoom_levels[default_zoom_index],
  };
};