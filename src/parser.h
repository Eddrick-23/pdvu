#pragma once
#include <memory>
#include <string>
#include <vector>

// Tell C++ compiler to treat these includes as C code
extern "C" {
#include <mupdf/fitz.h>
}

namespace pdf {
    using DisplayListHandle = std::shared_ptr<fz_display_list>;

    struct PageSpecs {
        int x0, y0, x1, y1;
        int width, height;
        size_t size;
        float acc_width, acc_height;
    };

    struct HorizontalBound {
        fz_rect rect;
        int width;
        int height;
        size_t bytes;
        size_t offset; // offset from start pointer of buffer
    };
    struct Parser {

        virtual ~Parser() = default;
        virtual void clear_doc() = 0;
        virtual bool load_document(const std::string& filepath) = 0;
        virtual const std::string& get_document_name() const = 0;
        virtual PageSpecs page_specs(int page, float zoom) const = 0;
        virtual std::vector<HorizontalBound> split_bounds(PageSpecs, int n) = 0;
        virtual int num_pages() const = 0;
        // virtual void write_page(int page_num, int w, int h,
        //                 float zoom, float rotate,
        //                 unsigned char* buffer, fz_rect clip) = 0;
        virtual pdf::DisplayListHandle get_display_list(int page_num) = 0;
        virtual void write_section(int page_num, int w, int h, float zoom, float rotate,
                            pdf::DisplayListHandle dlist, unsigned char* buffer,
                            fz_rect clip) = 0;
        virtual std::unique_ptr<Parser> duplicate() const = 0;
    };

    class MuPDFParser : public Parser{
    public:
        explicit MuPDFParser(bool use_ICC, fz_context* cloned_ctx = nullptr);
        ~MuPDFParser() override;

        // delete copy constructors
        MuPDFParser(const MuPDFParser&) = delete;
        MuPDFParser& operator=(const MuPDFParser&) = delete;

        // move constructors
        MuPDFParser(MuPDFParser&& other) noexcept;
        MuPDFParser& operator=(MuPDFParser&& other) noexcept;

        void clear_doc() override;
        bool load_document(const std::string& filepath) override;
        const std::string& get_document_name() const override;
        [[nodiscard]] PageSpecs page_specs(int page, float zoom) const override;
        [[nodiscard]] std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n) override;
        int num_pages() const override;
        // void write_page(int page_num, int w, int h,
        //                 float zoom, float rotate,
        //                 unsigned char* buffer, fz_rect clip) override;
        [[nodiscard]] DisplayListHandle get_display_list(int page_num) override;
        void write_section(int page_num, int w, int h, float zoom, float rotate,
                            DisplayListHandle dlist, unsigned char* buffer,
                            fz_rect clip) override;
        [[nodiscard]] std::unique_ptr<Parser> duplicate() const override;

    private:
        fz_context* ctx;
        fz_document* doc;
        std::string doc_name;
        std::string full_filepath;
        bool use_icc_profile;
    };
}
