#include<utils/lru_cache.h>
#include <gtest/gtest.h>

TEST(LRUCache, TestPutItem) {
    constexpr int cache_size = 1;
    auto cache = LRUCache<std::string, int>(cache_size);
    cache.put("test", 1);

    auto data = cache.get_entries();

    EXPECT_EQ(data.size(), 1);
}

TEST(LRUCache, TestPutMultipleItems) {
    struct TestCase{
        std::string key;
        int value;
    };
    constexpr std::array<TestCase, 3> test_cases = {{
        {"a", 1},
        {"b", 2},
        {"c", 3},
    }};

    constexpr int cache_size = test_cases.size();
    auto cache = LRUCache<std::string, int>(cache_size);
    for (auto& [key, value] : test_cases) {
        cache.put(key, value);
    }

    EXPECT_EQ(cache.get_entries().size(), test_cases.size());
}

TEST(LRUCache, TestGetItems) {
    struct TestCase{
        std::string key;
        int value;
    };
    constexpr std::array<TestCase, 3> test_cases = {{
        {"a", 1},
        {"b", 2},
        {"c", 3},
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
    struct TestCase{
        std::string key;
        int value;
    };
    constexpr std::array<TestCase, 2> test_cases = {{
        {"a", 1},
        {"b", 2},
    }};

    auto cache = LRUCache<std::string, int>(1);
    cache.put(test_cases[0].key, test_cases[0].value);

    EXPECT_EQ(cache.get(test_cases[0].key), test_cases[0].value);

    cache.put(test_cases[1].key, test_cases[1].value);
    EXPECT_FALSE(cache.get(test_cases[0].key).has_value());
    EXPECT_EQ(cache.get(test_cases[1].key), test_cases[1].value);
}

TEST(LRUCache, TestReorderOnGet) {
    struct TestCase{
        std::string key;
        int value;
    };
    constexpr std::array<TestCase, 2> test_cases = {{
        {"a", 1},
        {"b", 2},
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
    struct TestCase{
        std::string key;
        int value;
    };
    constexpr std::array<TestCase, 2> test_cases = {{
        {"a", 1},
        {"b", 2},
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