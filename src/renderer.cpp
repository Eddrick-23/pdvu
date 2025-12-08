#include <format>
#include "renderer.h"

// encode 3 bytes of data per chunk into 4 bytes
// divide 24 bits into 4 groups of 6
// Add two 00s in front of each group of 6 to expand to 32 bits
// Each byte is then mapped to a number less than 64 based on the lookup table
std::string base64_encode(const std::string& input) {
    static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

std::string get_image_sequence(const std::string& filepath,
                               int img_width, int img_height,
                               const std::string& transmission_medium) {
    std::string result = std::format("\033_Ga=T,t={},f=24,C=1,z=-1,s={},v={};{}\033\\",
                                 transmission_medium == "shm" ? "s" : "f" ,
                                 img_width,
                                 img_height,
                                 base64_encode(filepath));
    return result;
}
