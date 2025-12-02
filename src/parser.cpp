#include "parser.h"
#include <iostream>

Parser::Parser() {
    // 1. Initialize the MuPDF Context
    // NULL, NULL = standard memory allocators
    // FZ_STORE_DEFAULT = default resource cache size
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    doc = nullptr;
    current_pixmap = nullptr;

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

void Parser::clear_pixmap() {
    if (current_pixmap) {
        fz_drop_pixmap(ctx, current_pixmap);
        current_pixmap = nullptr;
    }
}

Parser::~Parser() {
    // Cleanup
    clear_doc();
    clear_pixmap();
    if (ctx) {
        fz_drop_context(ctx);
        ctx = nullptr;
    }
}

void Parser::init_test() {
    std::cout << "[System] MuPDF Context initialized successfully." << std::endl;
    std::cout << "[System] Linker verification passed." << std::endl;
}
bool Parser::load_document(const std::string& filepath) {
    fz_try(ctx) {
        doc = fz_open_document(ctx, filepath.c_str());
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Could not open file: " << filepath << std::endl;
        return false;
    }
    std::cout << "[System] Document loaded: " << filepath << std::endl;
    return true;
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
    // Render page to an RGB pixmap
    clear_pixmap(); // clear
    fz_try(ctx) {
        current_pixmap = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 0);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to create pixmap." << std::endl;
        fz_drop_page(ctx, page);
        return RawImage{};
    }
    unsigned char* raw_buffer = fz_pixmap_samples(ctx, current_pixmap);
    const int width = fz_pixmap_width(ctx, current_pixmap);
    const int height = fz_pixmap_height(ctx, current_pixmap);
    const int stride = fz_pixmap_stride(ctx, current_pixmap);
    return RawImage{raw_buffer, width, height, stride, stride * height};
}

