#include "parser.h"
#include <vector>
#include <filesystem>
#include <iostream>


Parser::Parser() {
    // NULL, NULL = standard memory allocators
    // FZ_STORE_DEFAULT = default resource cache size
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    doc = nullptr;

    if (!ctx) {
        std::cerr << "CRITICAL: Failed to create MuPDF context." << std::endl;
        exit(EXIT_FAILURE);
    }

    // 2. Register default document handlers (PDF, EPUB, etc.)
    fz_register_document_handlers(ctx);

}

void Parser::clear_doc() {
    if (doc) {
        fz_drop_document(ctx, doc);
        doc = nullptr;
    }
}


Parser::~Parser() {
    // Cleanup
    clear_doc();
    if (ctx) {
        fz_drop_context(ctx);
        ctx = nullptr;
    }
}

bool Parser::load_document(const std::string& filepath) {
    clear_doc();
    fz_try(ctx) {
        doc = fz_open_document(ctx, filepath.c_str());
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Could not open file: " << filepath << std::endl;
        return false;
    }
    std::filesystem::path p(filepath);
    doc_name = p.filename().string();
    return true;
}

const std::string& Parser::get_document_name() const{
    return doc_name;
}

int Parser::num_pages() const {
    if (!doc) {
        return 0;
    }
    int count = 0;
    fz_try(ctx) {
        count = fz_count_pages(ctx, doc);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to count pages." << std::endl;
        return 0;
    }
    return count;
}

PageBounds Parser::get_page_dimensions(const int page_num) const{
    fz_page* page = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to load page." << std::endl;
        return {0,0,0,0};
    }
    fz_rect bounds = fz_bound_page(ctx, page);
    fz_drop_page(ctx, page);
    return PageBounds(bounds.x0, bounds.y0, bounds.x1, bounds.y1);
}

RawImage Parser::get_page(const int page_num, const float zoom, const float rotate) {
    fz_page* page = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to load page." << std::endl;
        return RawImage{};
    }
    /* Compute a transformation matrix for the zoom and rotation desired. */
    /* default zoom is 100 -> 100% */
    /* The default resolution without scaling is 72 dpi. */
    fz_matrix ctm = fz_scale(zoom / 100, zoom / 100);
    ctm = fz_pre_rotate(ctm, rotate);
    fz_pixmap* raw_pix = nullptr;

    fz_try(ctx) {
        raw_pix = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 0);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to create pixmap." << std::endl;
        fz_drop_page(ctx, page);
        return RawImage{};
    }
    fz_drop_page(ctx, page);
    // creation of custom smart pointer
    fz_context* captured_context = ctx;
    auto deleter = [captured_context](fz_pixmap* p) {
        if (p) {
            fz_drop_pixmap(captured_context, p);
        }
    };
    PixMapPtr smart_pixmap(raw_pix, deleter);

    // make sure the struct owns the pixmap
    return RawImage{
        std::move(smart_pixmap),
        fz_pixmap_samples(ctx, raw_pix),
        fz_pixmap_width(ctx, raw_pix),
        fz_pixmap_height(ctx, raw_pix),
        fz_pixmap_stride(ctx, raw_pix),
        static_cast<size_t>(fz_pixmap_stride(ctx, raw_pix)) * fz_pixmap_height(ctx, raw_pix),
    };
}

Parser::Parser(Parser&& other) noexcept {
    ctx = other.ctx;
    doc = other.doc;
    doc_name = other.doc_name;

    other.ctx = nullptr;
    other.doc = nullptr;
    other.doc_name.clear();
}

Parser& Parser::operator=(Parser&& other) noexcept {
    if (this != &other) {
        // clear current processes
        clear_doc();
        if (ctx) {
            fz_drop_context(ctx);
        }

        ctx = other.ctx;
        doc = other.doc;
        doc_name = other.doc_name;

        other.ctx = nullptr;
        other.doc = nullptr;
        other.doc_name.clear();
    }
    return *this;
}

