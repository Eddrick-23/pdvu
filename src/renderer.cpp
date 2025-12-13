#include <format>
#include "renderer.h"

// encode 3 bytes of data per chunk into 4 bytes
// divide 24 bits into 4 groups of 6
// Add two 00s in front of each group of 6 to expand to 32 bits
// Each byte is then mapped to a number less than 64 based on the lookup table
namespace {
    constexpr std::string base64_encode(const std::string& input) {
        constexpr char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
        if (valb > -6) out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }
    constexpr std::string b64_black_pixel(int opacity) {
        std::string pixel;
        pixel.push_back(0);
        pixel.push_back(0);
        pixel.push_back(0);
        const int alpha_channel = opacity*255/100; // map from 0 to 100%
        pixel.push_back(alpha_channel);
        return base64_encode(pixel);
    }
}

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
                                 base64_encode(filepath));
    return result;
}

std::string get_dim_layer(int term_width, int term_height) {
    constexpr std::string b64_pixel = b64_black_pixel(100);
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
