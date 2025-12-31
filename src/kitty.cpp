#include "kitty.h"
#include "kitty_internal.h"
#include <format>
namespace kitty {
  static constexpr int32_t IMAGE_Z = INT32_MIN / 2 - 2;
  static constexpr int32_t DIM_LAYER_Z = INT32_MIN /2 - 1;

  std::string get_image_sequence(const std::string &filepath, int img_id,
                                 int img_width, int img_height,
                                 int x_offset_pixels, int y_offset_pixels,
                                 int crop_rect_width, int crop_rect_height,
                                 const std::string &transmission_medium,
                                 bool transmit) {
    // z-index below INT32_MIN / 2 to render under background colours
    // This allows rendering text over the image
    std::string sequence;
    if (transmit) {
      // save the full image first
      sequence += std::format(
          "\x1b_Ga=t,q=2,i={},t={},f=24,s={},v={};{}"
          "\x1b\\",
          img_id, transmission_medium == "shm" ? "s" : "f", img_width,
          img_height,
          kitty::detail::base64_encode(filepath));
    }
    sequence += std::format( // read and display
        "\x1b_Ga=p,q=2,i={},p={},C=1,z={},x={},y={},w={},h={}\x1b\\", img_id, img_id,
        IMAGE_Z,
        x_offset_pixels,   // offset right from top left
        y_offset_pixels,   // offset down from top left
        crop_rect_width,   // width of visible area -> rectangle width
        crop_rect_height); // height of visible area -> rectangle height
    return sequence;
  }

  std::string delete_image_placement() {
    return "\x1b_Ga=d\x1b\\"; // delete all visible placements
  }
  std::string get_dim_layer(int term_width, int term_height) {
    const std::string b64_block = detail::b64_black_pixel_3x3(100);
    // create a 3 x 3 block
    // crop out the middle pixel
    // use that pixel and stretch to prevent interpolation from affecting transparency
    std::string result =
        std::format("\x1b_Ga=T,q=2,f=32,C=1,z={},s={},v={},c={},r={},x={},y={},w={},h={};{}\x1b\\",
                    DIM_LAYER_Z, // below written layer
                    3, 3, term_width, term_height,
                    1, 1, 1, 1,
                    b64_block);
    return result;
  }



  std::string clear_dim_layer() {
    return std::format("\x1b_Ga=d,d=Z,z={}\x1b\\", DIM_LAYER_Z);
  }
}