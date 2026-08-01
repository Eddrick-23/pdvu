#include <gtest/gtest.h>

#include "render/bounds.h"
#include "render/pdf_constants.h"

using namespace pdf;

namespace {

/// simple fixture to set up page specs for input
PageSpecs make_page_specs(int x0, int y0, int width, int height) {  // NOLINT
  PageSpecs ps{};
  ps.x0 = x0;
  ps.y0 = y0;
  ps.x1 = x0 + width;
  ps.y1 = y0 + height;
  ps.width = width;
  ps.height = height;
  ps.size = static_cast<size_t>(width) * g_pad * static_cast<size_t>(height);
  ps.rotation = 0;
  return ps;
}
}  // namespace

// -----------------------------------------------------------
// Parameterized invariant tests
//
// Every pixel is accounted for exactly once.
// Strips are contiguous and correctly offset.
// Horizontal bounds are untouched (same width for all strips).
// -----------------------------------------------------------

struct SplitBoundsCase {
  std::string name;
  PageSpecs ps;
  int n;
};

class SplitBoundsInvariants : public ::testing::TestWithParam<SplitBoundsCase> {};

TEST_P(SplitBoundsInvariants, HoldsStructuralInvariants) {
  const auto& [name, ps, n] = GetParam();
  const auto strips = split_bounds(ps, n);

  ASSERT_EQ(strips.size(), static_cast<size_t>(n)) << "expected exactly n strips";

  int running_height = 0;
  size_t running_bytes = 0;

  for (size_t i = 0; i < strips.size(); i++) {
    const auto& s = strips[i];
    SCOPED_TRACE(::testing::Message() << "strip index: " << i);

    // Horizontal bounds are constant across every strip
    EXPECT_FLOAT_EQ(s.rect.x0, static_cast<float>(ps.x0));
    EXPECT_FLOAT_EQ(s.rect.x1, static_cast<float>(ps.x1));
    EXPECT_EQ(s.width, ps.x1 - ps.x0);

    // bytes = width * height * bytes-per-pixel (g_pad)
    EXPECT_EQ(s.bytes, static_cast<size_t>(s.width) * static_cast<size_t>(s.height) * g_pad);

    // strips are contiguous, y0 continues where the running total left off, following [start, end)
    // convention
    EXPECT_EQ(s.rect.y0, static_cast<float>(ps.y0 + running_height));
    EXPECT_EQ(s.rect.y1, static_cast<float>(ps.y0 + running_height + s.height));

    // Offsets are the exact running byte total of all prior strips
    EXPECT_EQ(s.offset, running_bytes);

    running_height += s.height;
    running_bytes += s.bytes;
  }

  // strip heights sum to page height correctly, strip byte sizes sum to
  // page's total buffer size exactly.
  EXPECT_EQ(running_height, ps.height) << "strip heights must sum to page height";
  EXPECT_EQ(running_bytes, ps.size) << "strip byte sizes must sum to page buffer size";
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(
    VariousPagesAndStripCounts, SplitBoundsInvariants,
    ::testing::Values(
      SplitBoundsCase{"EvenDivision_4Strips", make_page_specs(0, 0, 800, 1200), 4},
      SplitBoundsCase{"UnevenDivision_3Strips", make_page_specs(0, 0, 800, 1000), 3},
      SplitBoundsCase{"SingleStrip", make_page_specs(0, 0, 800, 1200), 1},
      SplitBoundsCase{"StripPerRow", make_page_specs(0, 0, 10, 10), 10},
      SplitBoundsCase{"WidePage", make_page_specs(0, 0, 5000, 40), 4},
      SplitBoundsCase{"TallPage", make_page_specs(0, 0, 40, 5000), 7},
      SplitBoundsCase{"NonZeroOrigin", make_page_specs(100, 250, 600, 900), 5},
      SplitBoundsCase{"LargeDimensions",make_page_specs(0, 0, 20000, 20000),16,}),
      [](const ::testing::TestParamInfo<SplitBoundsCase>& info) {return info.param.name;});
// clang-format on

// -----------------------------------------------------------
//  Named tests for specific, documented behaviours and edge
//  cases
// -----------------------------------------------------------

TEST(SplitBoundsEdgeCases, SingleStripCoversEntirePage) {
  const auto ps = make_page_specs(0, 0, 400, 300);
  const auto strips = split_bounds(ps, 1);

  ASSERT_EQ(strips.size(), 1U);
  EXPECT_EQ(strips[0].width, 400);
  EXPECT_EQ(strips[0].height, 300);
  EXPECT_FLOAT_EQ(strips[0].rect.x0, 0.0);
  EXPECT_FLOAT_EQ(strips[0].rect.y0, 0.0);
  EXPECT_FLOAT_EQ(strips[0].rect.x1, 400);
  EXPECT_FLOAT_EQ(strips[0].rect.y1, 300);
  EXPECT_EQ(strips[0].offset, 0);
  EXPECT_EQ(strips[0].bytes, ps.size);
}

TEST(SplitBoundsEdgeCases, NonPositiveNClampsToOne) {
  const auto ps = make_page_specs(0, 0, 400, 300);
  const auto strips = split_bounds(ps, 0);
  EXPECT_EQ(strips.size(), 1U);
  EXPECT_EQ(strips[0].height, 300);
  EXPECT_EQ(strips[0].bytes, ps.size);
}

TEST(SplitBoundsEdgeCase, StripCountEqualPageHeightGivesOnePixelRowsPerStrip) {
  const auto ps = make_page_specs(0, 0, 50, 20);
  const auto strips = split_bounds(ps, 20);

  ASSERT_EQ(strips.size(), 20U);
  for (const auto& s : strips) {
    EXPECT_EQ(s.height, 1);
  }
}

TEST(SplitBoundsEdgeCases, StripCountGreaterThanPageHeightClampsToPageHeightStrips) {
  const auto ps = make_page_specs(0, 0, 50, 20);
  const auto strips = split_bounds(ps, 30);

  ASSERT_EQ(strips.size(), 20U);
  for (const auto& s : strips) {
    EXPECT_EQ(s.height, 1);
  }
}

TEST(SplitBoundsEdgeCases, NegativeHeightClampsToSingleEmptyStrip) {
  auto ps = make_page_specs(0, 0, 400, 300);
  ps.height = -10;
  const auto strips = split_bounds(ps, 4);

  ASSERT_EQ(strips.size(), 1);
  EXPECT_EQ(strips[0].height, 0);
  EXPECT_EQ(strips[0].bytes, 0U);
  EXPECT_EQ(strips[0].offset, 0U);
  EXPECT_EQ(strips[0].rect.y0, strips[0].rect.y1);  // zero-height rect
}

TEST(SplitBoundsEdgeCases, NonPositiveNAndZeroHeightBothClamp) {
  auto ps = make_page_specs(0, 0, 400, 0);
  const auto strips = split_bounds(ps, -3);
  ASSERT_EQ(strips.size(), 1U);
  EXPECT_EQ(strips[0].height, 0);
}