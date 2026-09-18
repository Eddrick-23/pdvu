#include <gtest/gtest.h>

#include <array>

#include "render/PageRasterTransform.h"

namespace {
using QuarterTurn = pdf::PageRasterTransform::QuarterTurn;
}  // namespace

struct PointTransformCase {
  std::string name;
  QuarterTurn rotation;
  geometry::PixelSize raster_size;
  geometry::RasterPoint expected;
};

class TransformPointParameterizedTest : public ::testing::TestWithParam<PointTransformCase> {};

TEST_P(TransformPointParameterizedTest, TransformsPointForEveryQuarterTurn) {
  constexpr float zoom = 2.0F;
  constexpr geometry::PagePoint page_point{
      .x = 10.0F,
      .y = 20.0F,
  };

  const PointTransformCase& tc = GetParam();
  const pdf::PageRasterTransform transform(zoom, tc.rotation, tc.raster_size);

  const geometry::RasterPoint actual = transform.to_raster(page_point);

  EXPECT_FLOAT_EQ(actual.x, tc.expected.x);
  EXPECT_FLOAT_EQ(actual.y, tc.expected.y);
}

INSTANTIATE_TEST_SUITE_P(
    PageRasterTransform, TransformPointParameterizedTest,
    ::testing::Values(
        PointTransformCase{
            .name = "no_rotation",
            .rotation = QuarterTurn::Deg0,
            .raster_size = {.width = 200, .height = 400},
            .expected = {.x = 20.0F, .y = 40.0F},
        },
        PointTransformCase{
            .name = "quarter_clockwise",
            .rotation = QuarterTurn::Deg90,
            .raster_size = {.width = 400, .height = 200},
            .expected = {.x = 360.0F, .y = 20.0F},
        },
        PointTransformCase{
            .name = "upside_down",
            .rotation = QuarterTurn::Deg180,
            .raster_size = {.width = 200, .height = 400},
            .expected = {.x = 180.0F, .y = 360.0F},
        },
        PointTransformCase{
            .name = "three_quarters_clockwise",
            .rotation = QuarterTurn::Deg270,
            .raster_size = {.width = 400, .height = 200},
            .expected = {.x = 40.0F, .y = 180.0F},
        }),
    [](const ::testing::TestParamInfo<TransformPointParameterizedTest::ParamType>& info) {
      return info.param.name;
    });

TEST(PageRasterTransform, PreservesFractionalCoordinatesAtFractionalZoom) {
  constexpr float zoom = 0.67F;
  constexpr geometry::PagePoint page_point{
      .x = 10.0F,
      .y = 20.0F,
  };
  const pdf::PageRasterTransform transform(zoom, QuarterTurn::Deg90, {.width = 134, .height = 67});

  const geometry::RasterPoint actual = transform.to_raster(page_point);

  EXPECT_NEAR(actual.x, 120.6F, 0.0001F);
  EXPECT_NEAR(actual.y, 6.7F, 0.0001F);
}

TEST(PageRasterTransform, TransformsEveryQuadVertexWithoutReordering) {
  constexpr geometry::PageQuad page_quad{
      .upper_left = {.x = 10.0F, .y = 20.0F},
      .upper_right = {.x = 30.0F, .y = 20.0F},
      .lower_left = {.x = 10.0F, .y = 50.0F},
      .lower_right = {.x = 30.0F, .y = 50.0F},
  };
  const pdf::PageRasterTransform transform(2.0F, QuarterTurn::Deg90, {.width = 300, .height = 200});

  const geometry::RasterQuad actual = transform.to_raster(page_quad);

  EXPECT_FLOAT_EQ(actual.upper_left.x, 260.0F);
  EXPECT_FLOAT_EQ(actual.upper_left.y, 20.0F);
  EXPECT_FLOAT_EQ(actual.upper_right.x, 260.0F);
  EXPECT_FLOAT_EQ(actual.upper_right.y, 60.0F);
  EXPECT_FLOAT_EQ(actual.lower_left.x, 200.0F);
  EXPECT_FLOAT_EQ(actual.lower_left.y, 20.0F);
  EXPECT_FLOAT_EQ(actual.lower_right.x, 200.0F);
  EXPECT_FLOAT_EQ(actual.lower_right.y, 60.0F);
}

TEST(PageRasterTransformDegreeConversion, ConvertsCanonicalQuarterTurns) {
  struct DegreeConversionCase {
    int degrees;
    QuarterTurn expected;
  };

  constexpr std::array cases{
      DegreeConversionCase{.degrees = 0, .expected = QuarterTurn::Deg0},
      DegreeConversionCase{.degrees = 90, .expected = QuarterTurn::Deg90},
      DegreeConversionCase{.degrees = 180, .expected = QuarterTurn::Deg180},
      DegreeConversionCase{.degrees = 270, .expected = QuarterTurn::Deg270},
  };

  for (const auto& [degrees, expected] : cases) {
    SCOPED_TRACE(degrees);
    const auto actual = pdf::PageRasterTransform::quarter_turn_from_degrees(degrees);

    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(*actual, expected);
  }
}

TEST(PageRasterTransformDegreeConversion, RejectsNonCanonicalRotations) {
  constexpr std::array invalid_degrees{-90, 1, 45, 360};

  for (const int degrees : invalid_degrees) {
    SCOPED_TRACE(degrees);
    EXPECT_FALSE(pdf::PageRasterTransform::quarter_turn_from_degrees(degrees).has_value());
  }
}
