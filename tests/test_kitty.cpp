#include <gtest/gtest.h>

#include "kitty.h"
#include "kitty_internal.h"

using namespace kitty;

size_t count_substr(const std::string& str, const std::string& substr) {
  if (substr.empty()) return 0;
  size_t count = 0, pos = 0;
  while ((pos = str.find(substr, pos)) != std::string::npos) {
    count++;
    pos += substr.length();
  }
  return count;
}

// Test base64 encoding
TEST(KittyProtocolUtils, Base64Encoding) {
  struct {
    std::string input;
    std::string expected;
  } test_cases[] = {
      {"Hello", "SGVsbG8="}, {"World", "V29ybGQ="}, {"", ""}, {"foo", "Zm9v"}};
  for (const auto &[input, expected] : test_cases) {
    EXPECT_EQ(kitty::detail::base64_encode(input), expected);
  }
}

// Test black pixel generation
// TODO fix since we change to block
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
    EXPECT_EQ(kitty::detail::b64_black_pixel_3x3(opacity), expected);
  }
}

TEST(KittyProtocol, ImageSequence_StructureAndParameters_Transmit) {
  std::string result =
      get_image_sequence("testpath", 1, 600, 800, 0, 0, 0, 0, "shm", true);

  // Test escape characters
  EXPECT_TRUE(result.starts_with("\x1b_G"));
  EXPECT_TRUE(result.ends_with("\x1b\\"));

  // Test parameters
  EXPECT_EQ(count_substr(result, "q=2"), 2); // terminal quiet mode
  // transmission
  EXPECT_TRUE(result.contains("a=t"));   // Action = transmit without display
  EXPECT_TRUE(result.contains("i=1"));   // id = 1
  EXPECT_TRUE(result.contains("t=s"));   // shm transmission
  EXPECT_TRUE(result.contains("f=24"));  // rgb file format
  EXPECT_TRUE(result.contains("s=600")); // width
  EXPECT_TRUE(result.contains("v=800")); // height
  // placement

  EXPECT_TRUE(result.contains("a=t"));   // Action = transmit without display
  EXPECT_TRUE(result.contains("i=1"));   // id = 1
  EXPECT_TRUE(result.contains("p=1"));   // placement id = 1
  EXPECT_TRUE(result.contains("C=1"));   // keep cursor position
  EXPECT_TRUE(result.contains("z="));    // z index is set
  EXPECT_TRUE(result.contains("x=0"));   // crop x offset
  EXPECT_TRUE(result.contains("y=0"));   // crop y offset
  EXPECT_TRUE(result.contains("w=0"));   // crop rect width
  EXPECT_TRUE(result.contains("h=0"));   // crop rect height
}

TEST(KittyProtocol,
     KittyProtocol_ImageSequence_StructureAndParameters_NoTransmit) {
  std::string result =
      get_image_sequence("testpath", 1, 600, 800, 0, 0, 0, 0, "shm", false);

  EXPECT_TRUE(result.contains("i=1"));    // id = 1
  EXPECT_TRUE(result.contains("a=p"));    // Action = Placement
  EXPECT_FALSE(result.contains("t=s"));   // no transmission
  EXPECT_FALSE(result.contains("f=24"));  // no file format
  EXPECT_TRUE(result.contains("C=1"));    // keep cursor position
  EXPECT_TRUE(result.contains("z="));     // set z index
  EXPECT_FALSE(result.contains("s=600")); // width
  EXPECT_FALSE(result.contains("v=800")); // height
  EXPECT_TRUE(result.contains("x=0"));    // crop x offset
  EXPECT_TRUE(result.contains("y=0"));    // crop y offset
  EXPECT_TRUE(result.contains("w=0"));    // crop rect width
  EXPECT_TRUE(result.contains("h=0"));    // crop rect height
}

TEST(KittyProtocol, ImageSequence_TransmissionFormat) {
  auto result_shm =
      get_image_sequence("testfile", 1, 100, 100, 0, 0, 0, 0, "shm", true);
  auto result_tempfile =
      get_image_sequence("testfile", 1, 100, 100, 0, 0, 0, 0, "tempfile", true);

  EXPECT_TRUE(result_shm.contains("t=s"));
  EXPECT_TRUE(result_tempfile.contains("t=f"));
}

TEST(KittyProtocol, PathEncoding) {
  struct {
    std::string input;
    std::string expected;
  } test_paths[] = {{"shm_1234_0", "c2htXzEyMzRfMA=="},
                    {"/tmp/pdvu_28dn29", "L3RtcC9wZHZ1XzI4ZG4yOQ=="}};

  for (auto &[input, expected] : test_paths) {
    auto result =
        get_image_sequence(input, 1, 100, 100, 0, 0, 0, 0, "shm", true);
    EXPECT_TRUE(result.contains(expected));
  }
}
