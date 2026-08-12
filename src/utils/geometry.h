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

/**
 * @brief Point based on MuPDF page coordinates
 * Independent of viewer zoom/rotation, normally measured in MuPDf page units(72 dpi)
 */
struct PagePoint {
  float x;
  float y;
};

/**
 * @brief Point in a configured rendered bitmap
 * viewer zoom and rotation has been applied.
 */
struct RasterPoint {
  float x;
  float y;
};

/**
 * @brief Quad based on MuPDF page coordinates
 * Independent of viewer zoom/rotation, normally measured in MuPDf page units(72 dpi)
 */
struct PageQuad {
  PagePoint upper_left;
  PagePoint upper_right;
  PagePoint lower_left;
  PagePoint lower_right;
};

/**
 * @brief Quad in a configured rendered bitmap
 * viewer zoom and rotation has been applied.
 */
struct RasterQuad {
  RasterPoint upper_left;
  RasterPoint upper_right;
  RasterPoint lower_left;
  RasterPoint lower_right;
};

}  // namespace geometry