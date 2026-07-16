#include <gtest/gtest.h>
#include <utils/lru_cache.h>

#include <algorithm>

#include "gmock/gmock-matchers.h"

TEST(LRUCache, TestPutItem) {
  constexpr int cache_size = 1;
  auto cache = LRUCache<std::string, int>(cache_size);
  cache.put("test", 1);

  auto data = cache.get_entries();

  EXPECT_EQ(data.size(), 1);
}

TEST(LRUCache, TestPutMultipleItems) {
  struct TestCase {
    std::string key;
    int value;
  };
  std::array<TestCase, 3> test_cases = {{
      {.key = "a", .value = 1},
      {.key = "b", .value = 2},
      {.key = "c", .value = 3},
  }};

  constexpr int cache_size = test_cases.size();
  auto cache = LRUCache<std::string, int>(cache_size);
  for (auto& [key, value] : test_cases) {
    cache.put(key, value);
  }

  EXPECT_EQ(cache.get_entries().size(), test_cases.size());
}

TEST(LRUCache, TestGetItems) {
  struct TestCase {
    std::string key;
    int value;
  };
  const std::array<TestCase, 3> test_cases = {{
      {.key = "a", .value = 1},
      {.key = "b", .value = 2},
      {.key = "c", .value = 3},
  }};

  constexpr int cache_size = test_cases.size();
  auto cache = LRUCache<std::string, int>(cache_size);
  for (auto& [key, value] : test_cases) {
    cache.put(key, value);
  }
  EXPECT_FALSE(cache.get("d").has_value());
  EXPECT_EQ(cache.get("a"), 1);
}

TEST(LRUCache, TestEvict) {
  struct TestCase {
    std::string key;
    int value;
  };
  const std::array<TestCase, 2> test_cases = {{
      {.key = "a", .value = 1},
      {.key = "b", .value = 2},
  }};

  auto cache = LRUCache<std::string, int>(1);
  cache.put(test_cases[0].key, test_cases[0].value);

  EXPECT_EQ(cache.get(test_cases[0].key), test_cases[0].value);

  cache.put(test_cases[1].key, test_cases[1].value);
  EXPECT_FALSE(cache.get(test_cases[0].key).has_value());
  EXPECT_EQ(cache.get(test_cases[1].key), test_cases[1].value);
}

TEST(LRUCache, TestReorderOnGet) {
  struct TestCase {
    std::string key;
    int value;
  };
  std::array<TestCase, 2> test_cases = {{
      {.key = "a", .value = 1},
      {.key = "b", .value = 2},
  }};

  auto cache = LRUCache<std::string, int>(test_cases.size());
  for (auto& [key, value] : test_cases) {
    cache.put(key, value);
  }

  // last put should be at the front
  EXPECT_EQ(cache.get_entries()[0].key, test_cases[1].key);
  EXPECT_EQ(cache.get_entries()[0].value, test_cases[1].value);

  cache.get(test_cases[0].key);

  // key "a" should be the most recently used
  EXPECT_EQ(cache.get_entries()[0].key, test_cases[0].key);
  EXPECT_EQ(cache.get_entries()[0].value, test_cases[0].value);
}

TEST(LRUCache, TestReorderOnUpdate) {
  struct TestCase {
    std::string key;
    int value;
  };
  std::array<TestCase, 2> test_cases = {{
      {.key = "a", .value = 1},
      {.key = "b", .value = 2},
  }};

  auto cache = LRUCache<std::string, int>(test_cases.size());
  for (auto& [key, value] : test_cases) {
    cache.put(key, value);
  }

  // key "a" should be at the back
  EXPECT_EQ(cache.get_entries()[1].key, test_cases[0].key);
  EXPECT_EQ(cache.get_entries()[1].value, test_cases[0].value);

  cache.put("a", 300);

  // key "a" should be updated and pushed to front
  EXPECT_EQ(cache.get_entries()[0].key, "a");
  EXPECT_EQ(cache.get_entries()[0].value, 300);

  // key "b" should be moved to the back
  EXPECT_EQ(cache.get_entries()[1].key, test_cases[1].key);
  EXPECT_EQ(cache.get_entries()[1].value, test_cases[1].value);
}

struct EraseTestData {
  std::string name;
  std::vector<LRUCache<std::string, int>::Entry> entries;
  std::vector<LRUCache<std::string, int>::Entry> entries_after;
  std::string erase_key;
};

// REQUIRED FOR CTEST / CMAKE:
// Google Test uses Argument-Dependent Lookup (ADL) to print test parameters.
// Without this operator<<, gtest defaults to dumping the struct's raw memory as hex.
// CMake's test discovery (gtest_discover_tests) regex chokes on that hex dump
// and accidentally uses it as the test name in the console output.
// This function forces a clean string output, fixing the test names in CTest.
void PrintTo(const EraseTestData& data, std::ostream* os) { *os << data.name; }

void PrintTo(const LRUCache<std::string, int>::Entry& entry, std::ostream* os) {
  *os << "{key: " + entry.key + " " + "value: " + std::to_string(entry.value) + " }";
}

class EraseParameterizedTest : public ::testing::TestWithParam<EraseTestData> {};

TEST_P(EraseParameterizedTest, EraseCorrectly) {
  const EraseTestData& tc = GetParam();

  auto entries = tc.entries;  // create mutable copy
  auto cache = LRUCache<std::string, int>(tc.entries.size());
  std::ranges::reverse(entries.begin(), entries.end());
  for (const auto& [key, value] : entries) {
    cache.put(key, value);
  }
  cache.erase(tc.erase_key);
  EXPECT_THAT(cache.get_entries(), ::testing::ElementsAreArray(tc.entries_after));
}

INSTANTIATE_TEST_SUITE_P(
    LRUCache, EraseParameterizedTest,
    ::testing::Values(
        EraseTestData{
            "erase_front_element",
            {{"a", 1}, {"b", 2}},
            {{"b", 2}},
            "a",
        },
        EraseTestData{
            "erase_back_element",
            {{"a", 1}, {"b", 2}},
            {{"a", 1}},
            "b",
        },
        EraseTestData{
            "erase_middle_element",
            {{"a", 1}, {"b", 2}, {"c", 3}},
            {{"a", 1}, {"c", 3}},
            "b",
        },
        EraseTestData{
            "erase_missing_key",
            {{"a", 1}, {"b", 2}, {"c", 3}},
            {{"a", 1}, {"b", 2}, {"c", 3}},
            "d",
        },
        EraseTestData{
            "erase_empty_cache",
            {},
            {},
            "b",
        }),
    [](const ::testing::TestParamInfo<EraseParameterizedTest::ParamType>& info) {
      return info.param.name;
    });