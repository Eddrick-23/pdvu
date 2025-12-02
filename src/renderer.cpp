// handle terminal data and printing to terminal
#include <parser.h>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>

static const char base64_chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

constexpr int CHUNK_SIZE = 4096;

// encode 3 bytes of data per chunk into 4 bytes
// divide 24 bits into 4 groups of 6
// Add two 00s in front of each group of 6 to expand to 32 bits
// Each byte is then mapped to a number less than 64 based on the lookup table
// We will read from the buffer and update it directly.
inline void encode_block(const unsigned char* in, char* out, int len) {
    out[0] = base64_chars[(in[0] >> 2) & 0x3f];
    out[1] = base64_chars[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
    out[2] = (len > 1) ? base64_chars[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)] : '=';
    out[3] = (len > 2) ? base64_chars[in[2] & 0x3f] : '=';
}

void display_page(const RawImage& image) {
    unsigned char* data = image.pixels;
    size_t remaining = image.len;
    char outbuffer[CHUNK_SIZE+1];
    size_t current_size = 0;
    std::cout << "\033_G"
              << "a=T,f=24" << ","
              << "s=" << image.width << ","
              << "v=" << image.height << ";"; // don't render yet

    std::cout << std::flush;
    while (remaining > 0) {
        const size_t step = remaining >= 3 ? 3 : remaining; // 3 byte intervals
        // write to terminal one chunk at a time
        // we encode in 3 byte intervals, write to output
        // when we have accumulated the desired chunk size, write to terminal
        if (current_size + 4 > CHUNK_SIZE) {
            write(STDOUT_FILENO, outbuffer, current_size);
            current_size = 0;
        }
        unsigned char block[] = {0,0,0};
        for (int i = 0; i < step; i++) {
            block[i] = data[i];
        }
        // accumulate outbuffer
        encode_block(block, outbuffer + current_size, step);

        // Advance counters and pointers
        data += step; // progress buffer pointer
        current_size += 4;
        remaining -= step;
    }
    if (current_size > 0) {
        write(STDOUT_FILENO,  outbuffer, current_size);
        current_size = 0;
    }
    // flush buffer and display image
    std::cout << "\033\\" << std::flush;
}