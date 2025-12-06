#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

// smart pointer with custom deleter
using PixMapPtr = std::unique_ptr<fz_pixmap, std::function<void(fz_pixmap*)>>;
// using PixMapPtr = std::unique_ptr<fz_pixmap, std::function<void(fz_pixmap*)>>;

struct RawImage {
    // std::vector<unsigned char> pixels;
    PixMapPtr pixmap;
    unsigned char* pixels; // The raw pointer to the image buffer (Make deep copy if you want to cache!)
    int width;
    int height;
    int stride;            // Bytes per row (including padding)
    size_t len;            // Total size of the buffer
};

struct PageBounds {
    float x0, y0, x1, y1;
};

class Parser {
public:
    Parser();
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
    [[nodiscard]] PageBounds get_page_dimensions(int page) const;
    [[nodiscard]] int num_pages() const;
    [[nodiscard]] RawImage get_page(int page_num, float zoom, float rotate);


private:
    fz_context* ctx;
    fz_document* doc;
    std::string doc_name;
};