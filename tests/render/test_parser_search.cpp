#include <filesystem>

#include "gtest/gtest.h"
#include "render/parser.h"

namespace {
constexpr std::string_view g_fixtures_dir = PDVU_TEST_FIXTURES_DIR;

std::filesystem::path fixtures_dir() {
  static std::filesystem::path path{g_fixtures_dir};
  return path;
}

auto pdf_file_path(std::string_view filename) { return fixtures_dir() / "pdf" / filename; }

TEST(MuPDFSearchIntegration, FindsTextOnPage) {
  const auto parser = std::make_unique<pdf::MuPDFParser>(pdf::MuPDFParser(false));
  ASSERT_TRUE(parser->load_document(pdf_file_path("single_page.pdf")));
  const auto result = parser->search_page(0, "single_page");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1U);
}

TEST(MuPDFSearchIntegration, ReturnsEmptyResultsWhenTextIsAbsent) {
  const auto parser = std::make_unique<pdf::MuPDFParser>(pdf::MuPDFParser(false));
  ASSERT_TRUE(parser->load_document(pdf_file_path("single_page.pdf")));
  const auto result = parser->search_page(0, "");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 0);
}

TEST(MuPDFSearchIntegration, RejectsInvalidPage) {
  const auto parser = std::make_unique<pdf::MuPDFParser>(pdf::MuPDFParser(false));
  ASSERT_TRUE(parser->load_document(pdf_file_path("single_page.pdf")));
  const auto result = parser->search_page(10, "single_page");
  ASSERT_FALSE(result.has_value());
}
}  // namespace
