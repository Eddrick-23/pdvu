#include <gtest/gtest.h>

#include "utils/geometry.h"
#include "viewer/frame_layout.h"

namespace {

constexpr TermSize term_size{
    .columns = 100,
    .rows = 42,
    .pixel_width = 1000,
    .pixel_height = 840,
    .cell_pixel_width = 10,
    .cell_pixel_height = 20,
};

constexpr TUI::ContentArea content_area{
    .cols = 100,
    .rows = 40,
    .start_row = 2,
    .start_col = 1,
};

void expect_rect_eq(const geometry::PixelRect& actual, const geometry::PixelRect& expected) {
  EXPECT_EQ(actual.x, expected.x);
  EXPECT_EQ(actual.y, expected.y);
  EXPECT_EQ(actual.width, expected.width);
  EXPECT_EQ(actual.height, expected.height);
}

}  // namespace

TEST(FrameLayout, MatchingSourcePreservesCropAndCentersPlacement) {
  const geometry::PixelSize size{
      .width = 400,
      .height = 600,
  };
  const geometry::PixelRect crop{
      .x = 0,
      .y = 0,
      .width = 400,
      .height = 600,
  };

  const auto layout = viewer::calculate_frame_layout(size, size, crop, term_size, content_area);

  EXPECT_TRUE(layout.source_matches_target);
  expect_rect_eq(layout.target_crop_rect, crop);
  expect_rect_eq(layout.source_crop_rect, crop);

  EXPECT_EQ(layout.placement_cols, 40);
  EXPECT_EQ(layout.placement_rows, 30);

  // 400px = 40 cells; centered in 100 columns.
  EXPECT_EQ(layout.placement_origin.col, 31);

  // 600px = 30 cells; centered in 40 rows starting at row 2.
  EXPECT_EQ(layout.placement_origin.row, 7);
}

TEST(FrameLayout, MapsTargetCropIntoSmallerSourceBitmap) {
  const geometry::PixelSize source{
      .width = 400,
      .height = 600,
  };
  const geometry::PixelSize target{
      .width = 800,
      .height = 1200,
  };
  const geometry::PixelRect target_crop{
      .x = 200,
      .y = 300,
      .width = 400,
      .height = 600,
  };

  const auto layout =
      viewer::calculate_frame_layout(source, target, target_crop, term_size, content_area);

  EXPECT_FALSE(layout.source_matches_target);
  expect_rect_eq(layout.target_crop_rect, target_crop);

  expect_rect_eq(layout.source_crop_rect,
                 {
                     .x = 100,
                     .y = 150,
                     .width = 200,
                     .height = 300,
                 });

  // Placement describes the visible target crop, not the smaller source crop.
  EXPECT_EQ(layout.placement_cols, 40);
  EXPECT_EQ(layout.placement_rows, 30);

  // Origin is based on the 800x1200 target. It is too tall to center.
  EXPECT_EQ(layout.placement_origin.row, 2);
  EXPECT_EQ(layout.placement_origin.col, 11);
}

TEST(FrameLayout, MapsTargetCropIntoLargerSourceBitmap) {
  const geometry::PixelSize source{
      .width = 800,
      .height = 1200,
  };
  const geometry::PixelSize target{
      .width = 400,
      .height = 600,
  };
  const geometry::PixelRect target_crop{
      .x = 0,
      .y = 0,
      .width = 400,
      .height = 600,
  };

  const auto layout =
      viewer::calculate_frame_layout(source, target, target_crop, term_size, content_area);

  EXPECT_FALSE(layout.source_matches_target);
  expect_rect_eq(layout.target_crop_rect, target_crop);

  expect_rect_eq(layout.source_crop_rect,
                 {
                     .x = 0,
                     .y = 0,
                     .width = 800,
                     .height = 1200,
                 });

  EXPECT_EQ(layout.placement_cols, 40);
  EXPECT_EQ(layout.placement_rows, 30);

  EXPECT_EQ(layout.placement_origin.row, 7);
  EXPECT_EQ(layout.placement_origin.col, 31);
}

TEST(FrameLayout, RoundsPlacementDimensionsToNearestCell) {
  // Cells are 10x20 pixels.
  //
  // Width:  104 / 10 = 10.4 cells -> rounds down to 10.
  // Height: 211 / 20 = 10.55 cells -> rounds up to 11.
  const geometry::PixelSize size{
      .width = 104,
      .height = 211,
  };
  const geometry::PixelRect crop{
      .x = 0,
      .y = 0,
      .width = 104,
      .height = 211,
  };

  const auto layout = viewer::calculate_frame_layout(size, size, crop, term_size, content_area);

  EXPECT_TRUE(layout.source_matches_target);
  EXPECT_EQ(layout.placement_cols, 10);
  EXPECT_EQ(layout.placement_rows, 11);
}