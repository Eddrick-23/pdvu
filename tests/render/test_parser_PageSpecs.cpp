#include <gtest/gtest.h>

#include <cstddef>

#include "render/parser.h"

namespace {
// Mirrors fz_round_rect, which does NOT do nearest-rounding on each
// coordinate independently. It grows the rect outward so the integer
// rect fully contains the float rect: floor() on the lower bound (x0/y0),
// ceil() on the upper bound (x1/y1).
int round_lo(float v) { return static_cast<int>(std::floor(v)); }
int round_hi(float v) { return static_cast<int>(std::ceil(v)); }
}  // namespace

TEST(PageSpecsMethod, scale) {
  // use non whole bounds to test rounding
  pdf::PageSpecs ps{
      .base_x0 = 0,
      .base_y0 = 0,
      .base_x1 = 101,
      .base_y1 = 203,
      .x0 = 0,
      .y0 = 0,
      .x1 = 101,
      .y1 = 203,
      .width = 101,
      .height = 203,
      .size = static_cast<size_t>(3 * 101 * 203),
      .acc_width = 101.0,
      .acc_height = 203.0,
      .rotation = 0,
  };

  const std::array<float, 5> zoom_factors = {0.5, 0.67, 1.0, 1.5, 2.0};
  for (float zoom_factor : zoom_factors) {
    const pdf::PageSpecs scaled = ps.scale(zoom_factor);

    const float exp_x0 = ps.base_x0 * zoom_factor;
    const float exp_y0 = ps.base_y0 * zoom_factor;
    const float exp_x1 = ps.base_x1 * zoom_factor;
    const float exp_y1 = ps.base_y1 * zoom_factor;

    const int exp_rx0 = round_lo(exp_x0);
    const int exp_ry0 = round_lo(exp_y0);
    const int exp_rx1 = round_hi(exp_x1);
    const int exp_ry1 = round_hi(exp_y1);
    const int exp_width = std::abs(exp_rx1 - exp_rx0);
    const int exp_height = std::abs(exp_ry1 - exp_ry0);

    // base is untouched regardless of zoom
    EXPECT_FLOAT_EQ(scaled.base_x0, ps.base_x0);
    EXPECT_FLOAT_EQ(scaled.base_y0, ps.base_y0);
    EXPECT_FLOAT_EQ(scaled.base_x1, ps.base_x1);
    EXPECT_FLOAT_EQ(scaled.base_y1, ps.base_y1);

    // rounded integer bounds
    EXPECT_EQ(scaled.x0, exp_rx0);
    EXPECT_EQ(scaled.y0, exp_ry0);
    EXPECT_EQ(scaled.x1, exp_rx1);
    EXPECT_EQ(scaled.y1, exp_ry1);

    EXPECT_EQ(scaled.width, exp_width);
    EXPECT_EQ(scaled.height, exp_height);
    EXPECT_EQ(scaled.size, 3 * static_cast<size_t>(exp_width) * static_cast<size_t>(exp_height));

    // acc width and height not rounded. So use float math is fine.
    EXPECT_FLOAT_EQ(scaled.acc_width, exp_x1 - exp_x0);
    EXPECT_FLOAT_EQ(scaled.acc_height, exp_y1 - exp_y0);

    EXPECT_EQ(scaled.rotation, ps.rotation);
  }
}

TEST(PageSpecsMethod, rotate_quarter_clockwise) {
  pdf::PageSpecs ps{
      .base_x0 = 0,
      .base_y0 = 0,
      .base_x1 = 100,
      .base_y1 = 200,
      .x0 = 0,
      .y0 = 0,
      .x1 = 100,
      .y1 = 200,
      .width = 100,
      .height = 200,
      .size = static_cast<size_t>(3 * 100 * 200),
      .acc_width = 100.0,
      .acc_height = 200.0,
  };

  const std::array<int, 8> ns = {0, 1, 2, 3, 4, -1, -2, -5};
  const std::array<int, 8> expected_rotation = {0, 90, 180, 270, 0, 270, 180, 270};

  for (int i = 0; i < ns.size(); i++) {
    const int n = ns[i];
    const pdf::PageSpecs rotated = ps.rotate_quarter_clockwise(n);
    const bool flipped = std::abs(n) % 2 != 0;
    if (!flipped) {
      EXPECT_EQ(rotated.base_x0, ps.base_x0);
      EXPECT_EQ(rotated.base_y0, ps.base_y0);
      EXPECT_EQ(rotated.base_x1, ps.base_x1);
      EXPECT_EQ(rotated.base_y1, ps.base_y1);
      EXPECT_EQ(rotated.x0, ps.x0);
      EXPECT_EQ(rotated.y0, ps.y0);
      EXPECT_EQ(rotated.x1, ps.x1);
      EXPECT_EQ(rotated.y1, ps.y1);
      EXPECT_EQ(rotated.width, ps.width);
      EXPECT_EQ(rotated.height, ps.height);
      EXPECT_EQ(rotated.acc_width, ps.acc_width);
      EXPECT_EQ(rotated.acc_height, ps.acc_height);
    } else {  // expect x,y width,height to be flipped
      EXPECT_EQ(rotated.base_x0, ps.base_y0);
      EXPECT_EQ(rotated.base_y0, ps.base_x0);
      EXPECT_EQ(rotated.base_x1, ps.base_y1);
      EXPECT_EQ(rotated.base_y1, ps.base_x1);
      EXPECT_EQ(rotated.x0, ps.y0);
      EXPECT_EQ(rotated.y0, ps.x0);
      EXPECT_EQ(rotated.x1, ps.y1);
      EXPECT_EQ(rotated.y1, ps.x1);
      EXPECT_EQ(rotated.width, ps.height);
      EXPECT_EQ(rotated.height, ps.width);
      EXPECT_EQ(rotated.acc_width, ps.acc_height);
      EXPECT_EQ(rotated.acc_height, ps.acc_width);
    }
    EXPECT_EQ(rotated.size, ps.size);
    EXPECT_EQ(rotated.rotation, expected_rotation[i]);
  }
}
