#include "parser.h"
#include <gtest/gtest.h>

// TODO test for scaling
TEST(PageSpecsMethod, scale) {
  pdf::PageSpecs ps{0,   0,   100, 200,           0,     0,    100,
                    200, 100, 200, 3 * 100 * 200, 100.0, 200.0, 0};

  std::array<float, 4> zoom_factors = {1.0, 1.5, 2.0, 0.5};
  for (float zoom_factor: zoom_factors) {
    const pdf::PageSpecs scaled = ps.scale(zoom_factor);

    EXPECT_EQ(scaled.base_x0, ps.base_x0 * zoom_factor);
    EXPECT_EQ(scaled.base_y0, ps.base_y0 * zoom_factor);
    EXPECT_EQ(scaled.base_x1, ps.base_x1 * zoom_factor);
    EXPECT_EQ(scaled.base_y1, ps.base_y1 * zoom_factor);
    EXPECT_EQ(scaled.x0, ps.base_x0 * zoom_factor);
    EXPECT_EQ(scaled.y0, ps.base_y0 * zoom_factor);
    EXPECT_EQ(scaled.x1, ps.base_x1 * zoom_factor);
    EXPECT_EQ(scaled.y1, ps.base_y1 * zoom_factor);
    EXPECT_EQ(scaled.width, ps.width * zoom_factor);
    EXPECT_EQ(scaled.height, ps.height * zoom_factor);
    EXPECT_EQ(scaled.size, 3 * scaled.width * scaled.height);
    EXPECT_EQ(scaled.acc_width, ps.acc_width * zoom_factor);
    EXPECT_EQ(scaled.acc_height, ps.acc_height * zoom_factor);
    EXPECT_EQ(scaled.rotation, ps.rotation);
  }
}

// TODO test for rotation
TEST(PageSpecsMethod, rotate_quarter_clockwise) {
  pdf::PageSpecs ps{0,   0,   100, 200,           0,     0,    100,
                    200, 100, 200, 3 * 100 * 200, 100.0, 200.0};

  int rotation = 0;
  for (int i = 1; i <= 4; i++) {
    rotation = (rotation + 90) % 360;
    const pdf::PageSpecs rotated = ps.rotate_quarter_clockwise(i);
    if (i % 2 == 0) {
      EXPECT_EQ(rotated.base_x0, ps.base_x0);
      EXPECT_EQ(rotated.base_y0, ps.base_y0);
      EXPECT_EQ(rotated.base_x1, ps.base_x1);
      EXPECT_EQ(rotated.base_y1, ps.base_y1);
      EXPECT_EQ(rotated.x0, ps.base_x0);
      EXPECT_EQ(rotated.y0, ps.base_y0);
      EXPECT_EQ(rotated.x1, ps.base_x1);
      EXPECT_EQ(rotated.y1, ps.base_y1);
      EXPECT_EQ(rotated.width, ps.width);
      EXPECT_EQ(rotated.height, ps.height);
      EXPECT_EQ(rotated.size, ps.size);
      EXPECT_EQ(rotated.acc_width, ps.acc_width);
      EXPECT_EQ(rotated.acc_height, ps.acc_height);
      EXPECT_EQ(rotated.rotation, rotation);
    } else { // expect x,y width,height to be flipped
      EXPECT_EQ(rotated.base_x0, ps.base_y0);
      EXPECT_EQ(rotated.base_y0, ps.base_x0);
      EXPECT_EQ(rotated.base_x1, ps.base_y1);
      EXPECT_EQ(rotated.base_y1, ps.base_x1);
      EXPECT_EQ(rotated.x0, ps.base_y0);
      EXPECT_EQ(rotated.y0, ps.base_x0);
      EXPECT_EQ(rotated.x1, ps.base_y1);
      EXPECT_EQ(rotated.y1, ps.base_x1);
      EXPECT_EQ(rotated.width, ps.height);
      EXPECT_EQ(rotated.height, ps.width);
      EXPECT_EQ(rotated.size, ps.size);
      EXPECT_EQ(rotated.acc_width, ps.acc_height);
      EXPECT_EQ(rotated.acc_height, ps.acc_width);
      EXPECT_EQ(rotated.rotation, rotation);
    }
  }
}
