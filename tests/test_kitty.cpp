#include <gtest/gtest.h>

#include "kitty.h"
#include "kitty_internal.h"

// Test base64 encoding
TEST(KittyProtocolUtils, Base64Encoding) {
    struct {
        std::string input;
        std::string expected;
    } test_cases[] = {
        {"Hello", "SGVsbG8="},
        {"World", "V29ybGQ="},
        {"", ""},
        {"foo", "Zm9v"}
    };
    for (const auto& [input, expected] : test_cases) {
       EXPECT_EQ(kitty::detail::base64_encode(input), expected);
    }
}

// Test black pixel generation
TEST(KittyProtocolUtils, BlackPixelEncoding) {
    struct TestCase {
        int opacity;
        std::string expected;  // base64 of RGBA bytes [0,0,0,alpha]
    } test_cases[] = {
        {0,   "AAAAAA=="},  // [0,0,0,0]   -> fully transparent
        {50,  "AAAAfw=="},  // [0,0,0,127] -> half opacity
        {100, "AAAA/w=="},  // [0,0,0,255] -> fully opaque
        {25,  "AAAAPw=="},  // [0,0,0,63]  -> 25% opacity
        {75,  "AAAAvw=="},  // [0,0,0,191] -> 75% opacity
    };

    for (const auto& [opacity, expected] : test_cases) {
        EXPECT_EQ(kitty::detail::b64_black_pixel(opacity), expected);
    }
}

TEST(KittyProtocol, ImageSequence_StructureAndParameters) {
    std::string result = get_image_sequence("testpath", 600, 800, "shm");

    //Test escape characters
    EXPECT_TRUE(result.starts_with("\x1b_G"));
    EXPECT_TRUE(result.ends_with("\x1b\\"));

    //Test parameters
    EXPECT_TRUE(result.contains("a=T")); // Action = Transmit
    EXPECT_TRUE(result.contains("t=s")); // shm transmission
    EXPECT_TRUE(result.contains("f=24")); // rgb file format
    EXPECT_TRUE(result.contains("C=1")); // keep cursor position
    EXPECT_TRUE(result.contains("z=")); // set z index
    EXPECT_TRUE(result.contains("s=600")); // width
    EXPECT_TRUE(result.contains("v=800")); // height
}

TEST(KittyProtocol, ImageSequence_TransmissionFormat) {
    auto result_shm = get_image_sequence("testfile", 100, 100, "shm");
    auto result_tempfile = get_image_sequence("testfile", 100, 100, "tempfile");

    EXPECT_TRUE(result_shm.contains("t=s"));
    EXPECT_TRUE(result_tempfile.contains("t=f"));
}

TEST(KittyProtocol, PathEncoding) {
    struct {
        std::string input;
        std::string expected;
    } test_paths[] = {
        {"shm_1234_0", "c2htXzEyMzRfMA=="},
        {"/tmp/pdvu_28dn29" , "L3RtcC9wZHZ1XzI4ZG4yOQ=="}
    };

    for (auto& [input, expected] : test_paths) {
        auto result = get_image_sequence(input, 100, 100, "shm");
        EXPECT_TRUE(result.contains(expected));
    }
}

TEST(KittyProtocol, DimLayer_HasRequiredStructure) {
    auto result = get_dim_layer(80, 24);

    EXPECT_TRUE(result.starts_with("\x1b_G"));
    EXPECT_TRUE(result.ends_with("\x1b\\"));
    EXPECT_TRUE(result.contains("z=-1"));
    EXPECT_TRUE(result.contains("t=d"));
}

TEST(KittyProtocol, ClearDimLayer_MatchesExpectedFormat) {
    auto result = clear_dim_layer();

    EXPECT_EQ(result, "\x1b_Ga=d,d=Z,z=-1\x1b\\");
}