#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "gmock/gmock-matchers.h"
#include "render/parser.h"
#include "render/pdf_constants.h"

namespace {
constexpr std::string_view g_fixtures_dir = PDVU_TEST_FIXTURES_DIR;

std::filesystem::path fixtures_dir() {
  static std::filesystem::path path{g_fixtures_dir};
  return path;
}

auto pdf_file_path(std::string_view filename) { return fixtures_dir() / "pdf" / filename; }

pdf::PageSpecs make_page_specs(int width, int height, int rotation = 0) {
  return pdf::PageSpecs{
      .base_x0 = 0.0F,
      .base_y0 = 0.0F,
      .base_x1 = static_cast<float>(width),
      .base_y1 = static_cast<float>(height),
      .x0 = 0,
      .y0 = 0,
      .x1 = width,
      .y1 = height,
      .width = width,
      .height = height,
      .size = static_cast<size_t>(width * height * pdf::g_pad),
      .acc_width = static_cast<float>(width),
      .acc_height = static_cast<float>(height),
      .rotation = rotation,
  };
}

bool contains_rgb_pixel(const std::vector<unsigned char>& buffer,
                        const std::array<unsigned char, pdf::g_pad>& expected) {
  for (std::size_t i = 0; i + pdf::g_pad <= buffer.size(); i += pdf::g_pad) {
    if (buffer[i] == expected[0] && buffer[i + 1] == expected[1] && buffer[i + 1] == expected[2]) {
      return true;
    }
  }

  return false;
}

}  // namespace

TEST(MuPDFIntegration, MultiPagePDFReturnsCorrectMetadata) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  const bool res = p->load_document(pdf_file_path("multi_page.pdf"));

  ASSERT_TRUE(res) << "failed to load valid document";

  constexpr int expected_pages = 3;
  EXPECT_EQ(p->num_pages(), expected_pages);

  constexpr std::string_view expected_name = "multi_page.pdf";
  EXPECT_EQ(p->get_document_name(), expected_name);

  // check correct page_specs
  const std::array expected_page_specs = {
      make_page_specs(200, 300),
      make_page_specs(400, 100),
      make_page_specs(150, 150),
  };

  for (int i = 0; i < expected_pages; i++) {
    auto ps = p->page_specs(i);
    EXPECT_THAT(ps, testing::Optional(expected_page_specs[i]));
  }
}

TEST(MuPDFIntegration, EmptyFileNameOnUnloadedDoc) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  EXPECT_EQ(p->get_document_name(), "");
}

TEST(MuPDFIntegration, ZeroPagesOnUnloadedDoc) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  EXPECT_EQ(p->num_pages(), 0);
}

TEST(MuPDFIntegration, NullOptPageSpecsOnUnloadedDoc) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  EXPECT_EQ(p->page_specs(1), std::nullopt);
}

TEST(MuPDFIntegration, NullOptDisplayListOnUnloadedDoc) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  EXPECT_EQ(p->get_display_list(1), std::nullopt);
}

TEST(MuPDFIntegration, ClearDocProduceEmptyParser) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  EXPECT_TRUE(p->load_document(pdf_file_path("single_page.pdf")));

  p->clear_doc();

  EXPECT_EQ(p->get_document_name(), "");
  EXPECT_EQ(p->num_pages(), 0);
  EXPECT_EQ(p->page_specs(1), std::nullopt);
  EXPECT_EQ(p->get_display_list(1), std::nullopt);
}

TEST(MuPDFIntegration, FailedLoadProduceEmptyParser) {
  const std::unique_ptr<pdf::Parser> p = std::make_unique<pdf::MuPDFParser>(false);

  // first establish non empty state
  ASSERT_TRUE(p->load_document(pdf_file_path("single_page.pdf")));
  ASSERT_EQ(p->get_document_name(), "single_page.pdf");

  // load invalid pdf
  EXPECT_FALSE(p->load_document(pdf_file_path("not_a_pdf.pdf")));

  EXPECT_EQ(p->get_document_name(), "");
  EXPECT_EQ(p->num_pages(), 0);
  EXPECT_EQ(p->page_specs(1), std::nullopt);
  EXPECT_EQ(p->get_display_list(1), std::nullopt);
}

TEST(MuPDFIntegration, ThrowsExceptionWhenUsedAfterMove) {
  auto original_parser = pdf::MuPDFParser(false);

  EXPECT_TRUE(original_parser.load_document(pdf_file_path("multi_page.pdf")));

  const auto target_parser = std::move(original_parser);

  // original p should no longer hold context, and is in invalid state
  // calling methods that use the underlying context will result in runtime errors.
  EXPECT_EQ(original_parser.get_document_name(), "");  // doc_name is cached on load
  EXPECT_THROW(original_parser.load_document(pdf_file_path("multi_page.pdf")), std::runtime_error);
  EXPECT_THROW((void)original_parser.num_pages(), std::runtime_error);
  EXPECT_THROW((void)original_parser.page_specs(1), std::runtime_error);
  EXPECT_THROW((void)original_parser.get_display_list(1), std::runtime_error);
  EXPECT_THROW(original_parser.write_section(
                   100, 100, 1.0F, make_page_specs(100, 100), nullptr, nullptr, pdf::Rect{}),
               std::runtime_error);
}

TEST(MuPDFIntegration, MovedParserCanBeDuplicated) {
  // Test that a moved parser can call duplicate
  // and that duplicate is a valid parser (can still load pdfs)
  auto original_parser = pdf::MuPDFParser(false);

  EXPECT_TRUE(original_parser.load_document(pdf_file_path("multi_page.pdf")));

  const auto target_parser = std::move(original_parser);

  const auto duplicated_parser = target_parser.duplicate();

  EXPECT_EQ(duplicated_parser->get_document_name(), "multi_page.pdf");
  EXPECT_EQ(duplicated_parser->num_pages(), 3);

  // check correct page_specs on loading
  const std::array expected_page_specs = {
      make_page_specs(200, 300),
      make_page_specs(400, 100),
      make_page_specs(150, 150),
  };

  for (int i = 0; i < 3; i++) {
    auto ps = duplicated_parser->page_specs(i);
    EXPECT_THAT(ps, testing::Optional(expected_page_specs[i]));
  }
}

TEST(MuPDFIntegration, DisplayListCanOutliveParser) {
  // It should not be the case where a display list outlives a parser
  // but since it is exposed to the client side, we want to make sure
  // cleanup still happens even if the parser is destroyed first to prevent
  // leaks.

  std::optional<pdf::DisplayListHandle> dlist_handle;
  {
    auto p = std::make_unique<pdf::MuPDFParser>(false);
    ASSERT_TRUE(p->load_document(pdf_file_path("single_page.pdf")));
    dlist_handle = p->get_display_list(0);
    // parser goes out of scope here so it is destroyed
  }
  EXPECT_TRUE(dlist_handle.has_value());
  // calling reset will trigger the destructor since no one else holds a copy of the
  // shared pointer
  EXPECT_NO_FATAL_FAILURE(dlist_handle.reset());
}

TEST(MuPDFIntegration, RendersPageIntoRGBBuffer) {
  const auto p = std::make_unique<pdf::MuPDFParser>(false);
  ASSERT_TRUE(p->load_document(pdf_file_path("single_page.pdf")));
  const auto maybe_ps = p->page_specs(0);
  ASSERT_TRUE(maybe_ps.has_value());
  const auto ps = maybe_ps.value();

  const auto maybe_dlist = p->get_display_list(0);
  ASSERT_TRUE(maybe_dlist.has_value());
  const auto dlist_handle = maybe_dlist.value();

  std::vector<unsigned char> buffer(ps.size, 0xCD);
  p->write_section(ps.width,
                   ps.height,
                   1.0F,
                   ps,
                   dlist_handle,
                   buffer.data(),
                   pdf::Rect{
                       .x0 = static_cast<float>(ps.x0),
                       .y0 = static_cast<float>(ps.y0),
                       .x1 = static_cast<float>(ps.x1),
                       .y1 = static_cast<float>(ps.y1),
                   });
  constexpr std::array<unsigned char, pdf::g_pad> white{255, 255, 255};
  constexpr std::array<unsigned char, pdf::g_pad> black{0, 0, 0};

  // we don't do a full pixel hash comparison since it may be brittle. Just a simple check
  // to see if target pixel colours exist.
  EXPECT_TRUE(contains_rgb_pixel(buffer, white))
      << "rendered buffer should contain white page background";
  EXPECT_TRUE(contains_rgb_pixel(buffer, black))
      << "rendered buffer should contain fixture's black rectangle";
}

TEST(MuPDFIntegration, IntrinsicallyRotatedPageReportsDisplayedBounds) {
  pdf::MuPDFParser parser(false);
  ASSERT_TRUE(parser.load_document(pdf_file_path("rotated_page.pdf")));

  const auto specs = parser.page_specs(0);
  ASSERT_TRUE(specs.has_value());

  EXPECT_EQ(specs->width, 200);
  EXPECT_EQ(specs->height, 300);
  EXPECT_TRUE(parser.get_display_list(0).has_value());
}