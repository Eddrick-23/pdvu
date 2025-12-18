#include <format>
#include "kitty.h"
#include "kitty_internal.h"

std::string get_image_sequence(const std::string& filepath,
                               int img_width, int img_height,
                               const std::string& transmission_medium) {
    // z-index below INT32_MIN / 2 to render under background colours
    // This allows rendering text over the image
    constexpr int32_t UNDER_BG_Z = INT32_MIN/2 -1;

    std::string result = std::format("\x1b_Ga=T,t={},f=24,C=1,z={},s={},v={};{}\x1b\\",
                                 transmission_medium == "shm" ? "s" : "f" ,
                                 UNDER_BG_Z,
                                 img_width,
                                 img_height,
                                 kitty::detail::base64_encode(filepath));
    return result;
}

std::string get_dim_layer(int term_width, int term_height) {
    constexpr std::string b64_pixel = kitty::detail::b64_black_pixel(100);
    std::string result = std::format("\x1b_Ga=T,t=d,f=32,C=1,z={},s={},v={},c={},r={};{}\x1b\\",
        -1, // below written layer at index 1
        1,
        1,
        term_width,
        term_height,
        b64_pixel
    );
    return result;
}

std::string clear_dim_layer() { // clear the pixel we put at index -1
    // <ESC>_Ga=d,d=Z,z=-1<ESC>\
    // # delete the placements with z-index -1, also freeing up image data
    std::string result = std::format("\x1b_Ga=d,d=Z,z=-1\x1b\\");
    return result;
}
