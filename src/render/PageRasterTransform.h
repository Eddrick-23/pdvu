#pragma once
#include <optional>

#include "utils/geometry.h"

namespace pdf {
class PageRasterTransform {
 public:
  enum class QuarterTurn {
    Deg0,
    Deg90,
    Deg180,
    Deg270,
  };

  struct AffineCoefficients {
    float a;
    float b;
    float c;
    float d;
    float tx;
    float ty;
  };

  explicit PageRasterTransform(float zoom, QuarterTurn rotation, geometry::PixelSize output_size);
  [[nodiscard]] static std::optional<QuarterTurn> quarter_turn_from_degrees(int rotation_deg);
  [[nodiscard]] geometry::RasterPoint to_raster(const geometry::PagePoint& point) const;
  [[nodiscard]] geometry::RasterQuad to_raster(const geometry::PageQuad& quad) const;
  [[nodiscard]] AffineCoefficients coefficients() const;

 private:
  AffineCoefficients m_coefficients;
};
}  // namespace pdf