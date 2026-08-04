#include <gtest/gtest.h>
#include <viewer/pageview.h>

TEST(PageView, ChangeZoomIndexClampsCorrectly) {
  auto pageview = PageView();
  constexpr int zoom_levels = PageView::m_zoom_levels.size();
  pageview.change_zoom_index(zoom_levels * 2);
  EXPECT_EQ(pageview.current_zoom(), PageView::m_zoom_levels[zoom_levels - 1])
      << "Failed to clamp upper bound. Zoom levels size: " << zoom_levels;

  pageview.change_zoom_index(zoom_levels * -2);
  EXPECT_EQ(pageview.current_zoom(), PageView::m_zoom_levels[0])
      << "Failed to clamp lower bound. Zoom levels size: " << zoom_levels;
}

TEST(PageView, ChangeZoomIndexReturnsIfIndexChanged) {
  auto pageview = PageView();
  constexpr int zoom_levels = PageView::m_zoom_levels.size();
  EXPECT_TRUE(pageview.change_zoom_index(zoom_levels * 2));
  EXPECT_FALSE(pageview.change_zoom_index(1))
      << "Should indicate false as zoom index should be at upper limit";

  EXPECT_TRUE(pageview.change_zoom_index(zoom_levels * -2));
  EXPECT_FALSE(pageview.change_zoom_index(-1))
      << "Should indicate false as zoom index should be at lower limit";
}

TEST(PageView, ResetZoomIndex) {
  auto pageview = PageView();
  const float initial_zoom = pageview.current_zoom();
  EXPECT_FLOAT_EQ(PageView::m_zoom_levels[PageView::m_default_zoom_index], pageview.current_zoom());

  // Test zooming in if not already at max zoom
  if (PageView::m_default_zoom_index + 1 < PageView::m_zoom_levels.size()) {
    pageview.change_zoom_index(1);
    EXPECT_FLOAT_EQ(PageView::m_zoom_levels[PageView::m_default_zoom_index + 1],
                    pageview.current_zoom());

    pageview.reset_zoom_to_default();
    EXPECT_FLOAT_EQ(initial_zoom, pageview.current_zoom());
  }

  // Test zooming out if not already at min zoom
  if (PageView::m_default_zoom_index > 0) {
    pageview.change_zoom_index(-1);
    EXPECT_FLOAT_EQ(PageView::m_zoom_levels[PageView::m_default_zoom_index - 1],
                    pageview.current_zoom());

    pageview.reset_zoom_to_default();
    EXPECT_FLOAT_EQ(initial_zoom, pageview.current_zoom());
  }
}

struct PageViewTestAccessor {
  static float get_x_offset(const PageView& pv) { return pv.m_viewport.rel_x_offset; }
  static float get_y_offset(const PageView& pv) { return pv.m_viewport.rel_y_offset; }
  static void set_offsets(PageView& pv, float x_offset, float y_offset) {
    pv.m_viewport.rel_x_offset = x_offset;
    pv.m_viewport.rel_y_offset = y_offset;
  }
};

TEST(PageView, UpdateViewport) {
  PageView pageview;

  // Moving within bounds returns true and updates state
  PageViewTestAccessor::set_offsets(pageview, 0.0, 0.0);
  pageview.update_viewport(0.2F, 0.2F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.2F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.2F);

  // Zero movement
  PageViewTestAccessor::set_offsets(pageview, 0.5, 0.5);
  EXPECT_FALSE(pageview.update_viewport(0.0F, 0.0F));
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.5F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.5F);

  // Clamping to upper bounds
  EXPECT_TRUE(pageview.update_viewport(1.0F, 1.0F));
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 1.0F)
      << "Offset should clamp at max boundary (1.0) when delta exceeds bounds";
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 1.0F)
      << "Offset should clamp at max boundary (1.0) when delta exceeds bounds";

  // Clamping to lower bounds
  pageview.update_viewport(-1.5F, -1.5F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.0F)
      << "Offset should clamp at min boundary (0.0) when delta exceeds bounds";
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.0F)
      << "Offset should clamp at min boundary (0.0) when delta exceeds bounds";
}

TEST(PageView, UpdateViewportReturnsIfIndexChanged) {
  PageView pageview;

  // valid change
  PageViewTestAccessor::set_offsets(pageview, 0.0, 0.0);
  EXPECT_TRUE(pageview.update_viewport(0.5, 0.5));

  // valid change one dimension
  PageViewTestAccessor::set_offsets(pageview, 1.0, 0.0);
  EXPECT_TRUE(pageview.update_viewport(0.5, 0.5))
      << "should return true since y direction is not at limit";
  PageViewTestAccessor::set_offsets(pageview, 0.0, 1.0);
  EXPECT_TRUE(pageview.update_viewport(0.5, 0.5))
      << "should return true since x direction is not at limit";

  // Zero movement returns false (no change)
  EXPECT_FALSE(pageview.update_viewport(0.0, 0.0));

  // No change after values are clamped to upper bound
  PageViewTestAccessor::set_offsets(pageview, 1.0, 1.0);
  EXPECT_FALSE(pageview.update_viewport(1.0, 1.0));

  // No change after values are clamped to lower bound
  PageViewTestAccessor::set_offsets(pageview, 0.0, 0.0);
  EXPECT_FALSE(pageview.update_viewport(-1.0, -1.0));
}

TEST(PageView, ResetViewport) {
  PageView pageview;

  // check default state
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.5F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.5F);

  // at bottom limit
  PageViewTestAccessor::set_offsets(pageview, 0.0, 0.0);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.0F)
      << "x offset not set correctly";
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.0F)
      << "y offset not set correctly";

  // check if values are set to default
  pageview.reset_offsets_to_default();
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_x_offset(pageview), 0.5F);
  EXPECT_FLOAT_EQ(PageViewTestAccessor::get_y_offset(pageview), 0.5F);
}

struct CropWindowTestCase {
  std::string name;
  int img_width;
  int img_height;
  PageView::ViewportBounds bounds;
  float rel_x;
  float rel_y;
  PageView::CropRect expected;
};

class CalculateCropWindowTest : public ::testing::TestWithParam<CropWindowTestCase> {};

TEST_P(CalculateCropWindowTest, ComputesCorrectCrop) {
  const auto& param = GetParam();
  PageView pageview;

  // Set the viewport state before calculating the crop
  PageViewTestAccessor::set_offsets(pageview, param.rel_x, param.rel_y);

  const auto result =
      pageview.calculate_crop_window(param.img_width, param.img_height, param.bounds);

  EXPECT_EQ(param.expected.x_offset_pixels, result.x_offset_pixels);
  EXPECT_EQ(param.expected.y_offset_pixels, result.y_offset_pixels);
  EXPECT_EQ(param.expected.width, result.width);
  EXPECT_EQ(param.expected.height, result.height);
}

INSTANTIATE_TEST_SUITE_P(
    PageView, CalculateCropWindowTest,
    ::testing::Values(
        CropWindowTestCase{
            .name = "NoCropNeeded",
            .img_width = 800,
            .img_height = 600,
            // ViewportBounds: max_height_pixels, max_width_pixels
            .bounds = {1000, 1000},
            .rel_x = 0.5F,
            .rel_y = 0.5F,
            .expected = {0, 0, 800, 600},
        },
        CropWindowTestCase{
            .name = "CropTopLeft",
            .img_width = 2000,
            .img_height = 1500,
            .bounds = {1000, 800},
            .rel_x = 0.0F,
            .rel_y = 0.0F,
            .expected = {0, 0, 1000, 800},
        },
        CropWindowTestCase{
            .name = "CropBottomRight",
            .img_width = 2000,
            .img_height = 1500,
            .bounds = {1000, 800},
            .rel_x = 1.0F,
            .rel_y = 1.0F,
            // max_x_offset = 2000 - 1000 = 1000
            // max_y_offset = 1500 - 800 = 700
            .expected = {1000, 700, 1000, 800},
        },
        CropWindowTestCase{
            .name = "CropCentered",
            .img_width = 2000,
            .img_height = 1500,
            .bounds = {1000, 800},
            .rel_x = 0.5F,
            .rel_y = 0.5F,
            // max_x_offset = 1000 * 0.5 = 500
            // max_y_offset = 700 * 0.5 = 350
            .expected = {500, 350, 1000, 800},
        },
        CropWindowTestCase{
            .name = "CropWidthOnlyHeightClampsToImageHeight",
            .img_width = 2000,
            .img_height = 800,
            .bounds = {1000, 1000},
            .rel_x = 0.5F,
            .rel_y = 0.5F,
            // max_x_offset = 1000 * 0.5 = 500
            // max_y_offset = 0
            // no y offset, height should shrink to image height (smaller than given bounds)
            .expected = {500, 0, 1000, 800},
        },
        CropWindowTestCase{
            .name = "CropHeightOnlyWidthClampsToImageWidth",
            .img_width = 800,
            .img_height = 1500,
            .bounds = {1000, 800},
            .rel_x = 0.5F,
            .rel_y = 0.5F,
            // max_x_offset = 0
            // max_y_offset = 700 * 0.5 = 350
            // no x offset, width should shrink to image width (smaller than given bounds)
            .expected = {0, 350, 800, 800},
        }),
    [](const ::testing::TestParamInfo<CropWindowTestCase>& info) { return info.param.name; });