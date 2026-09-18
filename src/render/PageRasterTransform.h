#pragma once
#include <optional>

#include "utils/geometry.h"

namespace pdf {
/**
 * @brief Maps MuPDF page-space coordinates into rendered raster coordinates.
 *
 * Page-space coordinates describe locations on the unscaled, unrotated page. This transform
 * applies the same zoom and manual quarter-turn rotation used when rendering that page, producing
 * coordinates in the resulting bitmap. Keeping this calculation separate allows page content,
 * such as search-result quads, to follow the exact geometry used by the renderer.
 *
 * This class does not account for the bitmap's crop, pan offset, or terminal-cell placement.
 * Those operations are part of the viewer's FrameLayout calculation and are applied after this
 * page-to-raster transform.
 *
 * @note Raster coordinates remain floating point. Rounding to pixel or terminal-cell boundaries
 * is the responsibility of the code consuming the transformed geometry.
 */
class PageRasterTransform {
 public:
  /** @brief Supported clockwise manual page rotations. */
  enum class QuarterTurn {
    Deg0,
    Deg90,
    Deg180,
    Deg270,
  };

  /**
   * @brief Coefficients of the affine page-to-raster transform.
   *
   * A page point `(x, y)` is transformed as follows:
   * `x' = a*x + c*y + tx` and `y' = b*x + d*y + ty`.
   */
  struct AffineCoefficients {
    float a;   ///< Horizontal contribution from the page-space x coordinate.
    float b;   ///< Vertical contribution from the page-space x coordinate.
    float c;   ///< Horizontal contribution from the page-space y coordinate.
    float d;   ///< Vertical contribution from the page-space y coordinate.
    float tx;  ///< Horizontal translation in raster pixels.
    float ty;  ///< Vertical translation in raster pixels.
  };

  /**
   * @brief Builds the transform used to render a page at a given zoom and rotation.
   *
   * @param zoom Scale from MuPDF page units to raster pixels.
   * @param rotation Clockwise manual rotation applied by the renderer.
   * @param raster_size Final bitmap size after zoom and rotation. Its width and height provide
   * the translation needed to keep rotated coordinates inside the bitmap's positive bounds.
   */
  explicit PageRasterTransform(float zoom, QuarterTurn rotation, geometry::PixelSize raster_size);

  /**
   * @brief Converts a rotation in degrees to a supported quarter turn.
   *
   * @param rotation_deg Rotation in the canonical set `0`, `90`, `180`, or `270`.
   * @return The corresponding quarter turn, or `std::nullopt` for any other value.
   */
  [[nodiscard]] static std::optional<QuarterTurn> quarter_turn_from_degrees(int rotation_deg);

  /** @brief Transforms one page-space point into raster-space coordinates. */
  [[nodiscard]] geometry::RasterPoint to_raster(const geometry::PagePoint& point) const;

  /**
   * @brief Transforms every vertex of a page-space quad into raster-space coordinates.
   *
   * Vertex identities are preserved; the vertices are not reordered after rotation.
   */
  [[nodiscard]] geometry::RasterQuad to_raster(const geometry::PageQuad& quad) const;

  /** @brief Returns the affine coefficients represented by this transform. */
  [[nodiscard]] AffineCoefficients affine_coefficients() const;

 private:
  AffineCoefficients m_coefficients;
};
}  // namespace pdf
