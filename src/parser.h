#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

// // smart pointer with custom deleter
// using PixMapPtr = std::unique_ptr<fz_pixmap, std::function<void(fz_pixmap*)>>;
// // using PixMapPtr = std::unique_ptr<fz_pixmap, std::function<void(fz_pixmap*)>>;
//
// struct RawImage {
//     // std::vector<unsigned char> pixels;
//     PixMapPtr pixmap;
//     unsigned char* pixels; // The raw pointer to the image buffer (Make deep copy if you want to cache!)
//     int width;
//     int height;
//     int stride;            // Bytes per row (including padding)
//     size_t len;            // Total size of the buffer
// };

struct PageSpecs {
    int x0, y0, x1, y1;
    int width, height;
    size_t size;
};

class Parser {
public:
    explicit Parser(bool use_ICC);
    ~Parser();

    // delete copy constructors
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    // move constructors
    Parser(Parser&& other) noexcept;
    Parser& operator=(Parser&& other) noexcept;

    void clear_doc();
    bool load_document(const std::string& filepath);
    const std::string& get_document_name() const;
    // [[nodiscard]] PageBounds get_page_dimensions(int page) const;
    [[nodiscard]] PageSpecs page_specs(int page, float zoom) const;
    [[nodiscard]] int num_pages() const;
    // [[nodiscard]] RawImage get_page(int page_num, float zoom, float rotate);
    void write_page(int page_num, int w, int h,
                    float zoom, float rotate,
                    unsigned char* buffer, size_t buffer_len);

private:
    fz_context* ctx;
    fz_document* doc;
    std::string doc_name;
};