#include "parser.h"
#include <filesystem>
#include <iostream>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#endif
Parser::Parser(const bool use_ICC) {
    // NULL, NULL = standard memory allocators
    // FZ_STORE_DEFAULT = default resource cache size
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT/2);
    doc = nullptr;

    if (!ctx) {
        std::cerr << "CRITICAL: Failed to create MuPDF context." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Disable ICC colour management for performance
    if (!use_ICC) {
        fz_try(ctx) {
            fz_disable_icc(ctx);
        }
        fz_catch(ctx) {
            std::cerr << "WARNING: Failed to configure ICC." << std::endl;
        }
    }
    use_icc_profile = use_ICC;

    // fz_try(ctx) { // configuring anti-aliasing level
    //     // fz_set_aa_level(ctx, 0);
    //     fz_set_text_aa_level(ctx, 0);
    //     fz_set_graphics_aa_level(ctx, 0);
    // }
    // fz_catch(ctx) {
    //     std::cerr << "WARNING: Failed to configure AAC." << std::endl;
    // }

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
    full_filepath = filepath; // save path for duplicating
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

PageSpecs Parser::page_specs(const int page_num, const float zoom) const{
    ZoneScoped;
    fz_page* page = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to load page." << std::endl;
        return {0,0,0,0};
    }
    const fz_matrix ctm = fz_scale(zoom, zoom);
    fz_rect bounds = fz_bound_page(ctx, page);
    bounds =  fz_transform_rect(bounds, ctm);
    float acc_height = bounds.y1 - bounds.y0;
    float acc_width = bounds.x1 - bounds.x0;
    const fz_irect bbox = fz_round_rect(bounds);
    fz_drop_page(ctx, page);

    const int w = bbox.x1 - bbox.x0;
    const int h = bbox.y1 - bbox.y0;
    const size_t size = w * 3 * h;
    return PageSpecs(bbox.x0, bbox.y0, bbox.x1, bbox.y1, w, h, size, acc_width, acc_height);
}

void Parser::write_page(const int page_num, const int w, const int h,
                        const float zoom, const float rotate,
                        unsigned char* buffer, size_t buffer_len) {
    // ZoneScoped;
    // write page to a custom buffer directly
    fz_page* page = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to load page." << std::endl;
    }

    // check if buffer_len matches how much mupdf needs to write
    size_t required_len = static_cast<size_t>(w) * 3 * h; // stride * height
    if (required_len > buffer_len) {
        std::cerr <<"ERROR: Buffer too small. Given: " << buffer_len << " Required: " << required_len << std::endl;
        fz_drop_page(ctx, page);
        return; // abort the write
    }

    fz_pixmap* pix = nullptr;
    fz_try(ctx) {
        fz_matrix ctm = fz_scale(zoom, zoom);
        ctm = fz_pre_rotate(ctm, rotate);
        pix = fz_new_pixmap_with_data(ctx, fz_device_rgb(ctx), w, h,
                                                 NULL, 0, w * 3, buffer);
        fz_clear_pixmap_with_value(ctx, pix, 255); // set white background
        fz_device* dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_run_page(ctx, page, dev, ctm, nullptr);

        // free memory
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        fz_drop_page(ctx, page);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
        fz_drop_page(ctx, page);
        std::cerr <<"ERROR: Failed to draw page." << std::endl;
    }
}

std::unique_ptr<Parser> Parser::duplicate() const {
    auto clone = std::make_unique<Parser>(this -> use_icc_profile);
    // TODO duplicate the context instead of making a fresh one
    // might have to allow an optional context parameter in the constructor

    if (!full_filepath.empty()) {
        if (!clone->load_document(this->full_filepath)) {
            throw std::runtime_error("Failed to load document for cloned parser");
        }
    }
    return clone;
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