#pragma once
namespace geometry {
/**
 * @brief Defines height and width in terms of pixels
 */
struct PixelSize {
  int width;
  int height;
};

/**
 * @brief Defines a rectangle in terms of pixels
 *
 * @note x, y define the top left corner
 */
struct PixelRect {
  int x;
  int y;
  int width;
  int height;
};

/**
 * @brief Defines a terminal cell position
 *
 * @note Uses 1-based indexing.
 * (1, 1) refers to the top left terminal cell
 */
struct CellPosition {
  int row;
  int col;
};
}  // namespace geometry