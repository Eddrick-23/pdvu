#pragma once

#include <string>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

struct RawImage {
    unsigned char* pixels; // The raw pointer to the image buffer
    int width;
    int height;
    int stride;            // Bytes per row (including padding)
    int len;            // Total size of the buffer
};

struct PageBounds {
    float x0, y0, x1, y1;
};

class Parser {
public:
    Parser();
    ~Parser();

    // Just a test function for now
    void init_test();
    void clear_doc();
    void clear_pixmap();
    bool load_document(const std::string& filepath);
    [[nodiscard]] PageBounds get_page_dimensions(int page) const;
    [[nodiscard]] int num_pages() const;
    [[nodiscard]] RawImage get_page(int page_num, float zoom, float rotate);


private:
    fz_context* ctx;
    fz_document* doc;
    fz_pixmap* current_pixmap;
};