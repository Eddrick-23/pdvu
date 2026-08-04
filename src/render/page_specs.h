#pragma once
#include <cmath>
#include <compare>
#include <cstddef>

extern "C" {
#include <mupdf/fitz.h>
}

namespace pdf {
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

  auto operator<=>(const PageSpecs&) const = default;

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
 * @brief defines a rectangle with (x0, y0) at the top left and (x1, y1) at the bottom right.
 */
struct Rect {
  float x0, y0, x1, y1;
};

/**
 * @brief Represents a horizontal slice of a page for parallel processing.
 */
struct HorizontalBound {
  Rect rect;      ///< The MuPDF rectangle defining the physical bounds of this strip.
  int width;      ///< Pixel width of the strip.
  int height;     ///< Pixel height of the strip.
  size_t bytes;   ///< Total bytes contained in this strip.
  size_t offset;  ///< Memory offset in bytes from the start of the primary buffer.
};
}  // namespace pdf