#include "PageRasterTransform.h"

namespace pdf {
PageRasterTransform::PageRasterTransform(float zoom, QuarterTurn rotation,
                                         geometry::PixelSize output_size) {
  // a = s * cos(theta)
  // c = -s * sin(theta)
  // b = s * sin(theta)
  // d = s * cos(theta)
  // x' = a*x + c*y + tx
  // y' = b*x + d*y + ty
  switch (rotation) {
    case QuarterTurn::Deg0:
      // x' = x * scale
      // y' = y * scale
      m_coefficients = {
          .a = zoom,
          .b = 0,
          .c = 0,
          .d = zoom,
          .tx = 0,
          .ty = 0,
      };
      break;
    case QuarterTurn::Deg90:
      m_coefficients = {
          .a = 0,
          .b = zoom,
          .c = -zoom,
          .d = 0,
          .tx = static_cast<float>(output_size.width),
          .ty = 0,
      };
      break;
    case QuarterTurn::Deg180:
      m_coefficients = {
          .a = -zoom,
          .b = 0,
          .c = 0,
          .d = -zoom,
          .tx = static_cast<float>(output_size.width),
          .ty = static_cast<float>(output_size.height),
      };
      break;
    case QuarterTurn::Deg270:
      m_coefficients = {
          .a = 0,
          .b = -zoom,
          .c = zoom,
          .d = 0,
          .tx = 0,
          .ty = static_cast<float>(output_size.height),
      };
      break;
  }
}

geometry::RasterPoint PageRasterTransform::to_raster(const geometry::PagePoint& point) const {
  const auto [a, b, c, d, tx, ty] = m_coefficients;
  return geometry::RasterPoint{
      .x = (point.x * a) + (point.y * c) + tx,
      .y = (point.x * b) + (point.y * d) + ty,
  };
}

geometry::RasterQuad PageRasterTransform::to_raster(const geometry::PageQuad& quad) const {
  return geometry::RasterQuad{
      .upper_left = to_raster(quad.upper_left),
      .upper_right = to_raster(quad.upper_right),
      .lower_left = to_raster(quad.lower_left),
      .lower_right = to_raster(quad.lower_right),
  };
}

PageRasterTransform::AffineCoefficients PageRasterTransform::coefficients() const {
  return m_coefficients;
}

std::optional<PageRasterTransform::QuarterTurn> PageRasterTransform::quarter_turn_from_degrees(
    int rotation_deg) {
  using enum QuarterTurn;
  switch (rotation_deg) {
    case 0:
      return Deg0;
    case 90:
      return Deg90;
    case 180:
      return Deg180;
    case 270:
      return Deg270;
    default:
      return std::nullopt;
  }
}
}  // namespace pdf