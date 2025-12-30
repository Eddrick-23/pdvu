#pragma once
#include <string>

namespace kitty::detail {
// encode 3 bytes of data per chunk into 4 bytes
// divide 24 bits into 4 groups of 6
// Add two 00s in front of each group of 6 to expand to 32 bits
// Each byte is then mapped to a number less than 64 based on the lookup table
constexpr std::string base64_encode(const std::string &input) {
  constexpr char lookup[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.length() + 2) / 3) * 4);

  int val = 0;
  int valb = -6;
  for (unsigned char c : input) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(lookup[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4)
    out.push_back('=');
  return out;
}
constexpr std::string b64_black_pixel(int opacity) {
  std::string pixel;
  pixel.push_back(0);
  pixel.push_back(0);
  pixel.push_back(0);
  const int alpha_channel = opacity * 255 / 100; // map from 0 to 100%
  pixel.push_back(alpha_channel);
  return base64_encode(pixel);
}
} // namespace kitty::detail
