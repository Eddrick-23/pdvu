#pragma once
#include <memory>
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

struct IParser {

    virtual ~IParser() = default;
    virtual void clear_doc() = 0;
    virtual bool load_document(const std::string& filepath) = 0;
    virtual const std::string& get_document_name() const = 0;

    virtual PageSpecs page_specs(int page, float zoom) const = 0;
    virtual int num_pages() const = 0;
    virtual void write_page(int page_num, int w, int h,
                    float zoom, float rotate,
                    unsigned char* buffer, size_t buffer_len) = 0;
    virtual void write_section(int page_num, int w, int h, float zoom, float rotate,
                        fz_display_list* dlist, unsigned char* buffer,
                        fz_rect clip) = 0;
    virtual std::unique_ptr<IParser> duplicate() const = 0;
};

class Parser : public IParser{
public:
    explicit Parser(bool use_ICC, fz_context* cloned_ctx = nullptr);
    ~Parser() override;

    // delete copy constructors
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    // move constructors
    Parser(Parser&& other) noexcept;
    Parser& operator=(Parser&& other) noexcept;

    void clear_doc() override;
    bool load_document(const std::string& filepath) override;
    const std::string& get_document_name() const override;
    [[nodiscard]] PageSpecs page_specs(int page, float zoom) const override;
    int num_pages() const override;
    void write_page(int page_num, int w, int h,
                    float zoom, float rotate,
                    unsigned char* buffer, size_t buffer_len) override;
    void write_section(int page_num, int w, int h, float zoom, float rotate,
                        fz_display_list* dlist, unsigned char* buffer,
                        fz_rect clip) override;
    [[nodiscard]] std::unique_ptr<IParser> duplicate() const override;

private:
    fz_context* ctx;
    fz_document* doc;
    std::string doc_name;
    std::string full_filepath;
    bool use_icc_profile;
};