#include <gtest/gtest.h>
#include "../src/kitty_internal.h"

// Test base64 encoding
TEST(KittyProtocolTest, Base64Encoding) {
    std::string input = "Hello";
    std::string expected = "SGVsbG8=";
    EXPECT_EQ(kitty::detail::base64_encode(input), expected);
}