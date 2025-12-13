#pragma once
#include <string>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

struct PageSpecs {
    int x0, y0, x1, y1;
    int width, height;
    size_t size;
    float acc_width, acc_height;
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
    [[nodiscard]] PageSpecs page_specs(int page, float zoom) const;
    [[nodiscard]] int num_pages() const;
    void write_page(int page_num, int w, int h,
                    float zoom, float rotate,
                    unsigned char* buffer, size_t buffer_len);
    std::unique_ptr<Parser> duplicate() const;

private:
    fz_context* ctx;
    fz_document* doc;
    std::string doc_name;
    std::string full_filepath;
    bool use_icc_profile;
};