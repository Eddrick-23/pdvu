#include "parser.h"
#include <filesystem>
#include <iostream>
#include <mutex>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#endif

struct MutexLocks {
    std::mutex mutexes[FZ_LOCK_MAX];
};

static MutexLocks global_mu_locks;

// Callback: Lock
void lock_callback(void *user, int lock) {
    static_cast<MutexLocks*>(user)->mutexes[lock].lock();
}

// Callback: Unlock
void unlock_callback(void *user, int lock) {
    static_cast<MutexLocks*>(user)->mutexes[lock].unlock();
}
using namespace pdf;

MuPDFParser::MuPDFParser(const bool use_ICC, fz_context* cloned_ctx) {
    // FZ_STORE_DEFAULT = default resource cache size
    if (cloned_ctx) { // optional param to use a cloned context for duplicating
        ctx = std::move(cloned_ctx);
    } else {
        static fz_locks_context locks_ctx;
        locks_ctx.user = &global_mu_locks;
        locks_ctx.lock = lock_callback;
        locks_ctx.unlock = unlock_callback;
        ctx = fz_new_context(NULL, &locks_ctx, FZ_STORE_DEFAULT);
        fz_register_document_handlers(ctx);
    }
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


}

void MuPDFParser::clear_doc() {
    if (doc) {
        fz_drop_document(ctx, doc);
        doc = nullptr;
    }
}


MuPDFParser::~MuPDFParser() {
    // Cleanup
    clear_doc();
    if (ctx) {
        fz_drop_context(ctx);
        ctx = nullptr;
    }
}

bool MuPDFParser::load_document(const std::string& filepath) {
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

const std::string& MuPDFParser::get_document_name() const{
    return doc_name;
}

int MuPDFParser::num_pages() const {
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

PageSpecs MuPDFParser::page_specs(const int page_num) const{
    ZoneScoped;
    fz_page* page = nullptr;
    constexpr float base_zoom = 1.0;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr <<"ERROR: Failed to load page." << std::endl;
        return {0,0,0,0};
    }
    const fz_matrix ctm = fz_scale(base_zoom, base_zoom);
    // raw data
    fz_rect raw_bounds = fz_bound_page(ctx, page);
    raw_bounds =  fz_transform_rect(raw_bounds, ctm);

    // round to int
    const fz_irect bbox = fz_round_rect(raw_bounds);
    fz_drop_page(ctx, page);

    // dimensions
    const int w = bbox.x1 - bbox.x0;
    const int h = bbox.y1 - bbox.y0;
    const size_t size = w * 3 * h;
    float acc_height = raw_bounds.y1 - raw_bounds.y0;
    float acc_width = raw_bounds.x1 - raw_bounds.x0;

    return PageSpecs(
        raw_bounds.x0, raw_bounds.y0, raw_bounds.x1, raw_bounds.y1, // Base
        bbox.x0, bbox.y0, bbox.x1, bbox.y1, // ints
        w, h, size, acc_width, acc_height); // dims
}

std::vector<HorizontalBound> MuPDFParser::split_bounds(PageSpecs ps, int n) {
    ZoneScoped;
    // split the page into n horizontal strips represented by fz_rect
    constexpr int pad = 3; // data is in rgb format
    std::vector<HorizontalBound> bounds;
    int y_step = ps.height / n;
    int remainder = ps.height % n;
    const int x0 = ps.x0;
    const int x1 = ps.x1;
    // only y0 and y1 will be shifted
    int y0 = ps.y0;
    int y1 = ps.y0 + y_step;
    size_t offset = 0;
    for (int i = 0; i < n - 1; i++) {
        const size_t pixels = (x1 - x0) * (y1 - y0) * pad;
        auto data = HorizontalBound{fz_make_rect(x0, y0, x1, y1),x1 - x0, y1 - y0, pixels, offset};
        bounds.push_back(data);
        y0 = y1;
        y1 += y_step;
        offset += pixels;
    }
    // last iteration
    y1 += remainder;
    const size_t pixels = (x1 - x0) * (y1 - y0) * pad;
    const auto data = HorizontalBound{fz_make_rect(x0, y0, x1, y1), x1 - x0, y1 - y0,pixels, offset};
    bounds.push_back(data);
    return bounds;
}


// void MuPDFParser::write_page(const int page_num, const int w, const int h,
//                         const float zoom, const float rotate,
//                         unsigned char* buffer, fz_rect clip) {
//     ZoneScoped;
//     // write page to a custom buffer directly
//     fz_page* page = nullptr;
//     fz_try(ctx) {
//         page = fz_load_page(ctx, doc, page_num);
//     }
//     fz_catch(ctx) {
//         std::cerr <<"ERROR: Failed to load page." << std::endl;
//     }
//
//     if (w != clip.x1 - clip.x0 || h != clip.y1 - clip.y0) {
//         std::cerr << "ERROR: clip dimensions do not match w and h." << std::endl;
//         return;
//     }
//
//     fz_pixmap* pix = nullptr;
//
//     fz_try(ctx) {
//         fz_matrix ctm = fz_scale(zoom, zoom);
//         ctm = fz_pre_rotate(ctm, rotate);
//         pix = fz_new_pixmap_with_data(ctx, fz_device_rgb(ctx), w, h,
//                                                  NULL, 0, w * 3, buffer);
//         // set start points based on clip
//         pix->x = static_cast<int>(clip.x0);
//         pix->y = static_cast<int>(clip.y0);
//         fz_clear_pixmap_with_value(ctx, pix, 255); // set white background
//         fz_device* dev = fz_new_draw_device(ctx, fz_identity, pix);
//         fz_run_page(ctx, page, dev, ctm, nullptr);
//
//         // free memory
//         fz_close_device(ctx, dev);
//         fz_drop_device(ctx, dev);
//         fz_drop_page(ctx, page);
//         fz_drop_pixmap(ctx, pix);
//     }
//     fz_catch(ctx) {
//         if (pix) {
//             fz_drop_pixmap(ctx, pix);
//         }
//         fz_drop_page(ctx, page);
//         std::cerr <<"ERROR: Failed to draw page." << std::endl;
//     }
// }

using DisplayListHandle = std::shared_ptr<fz_display_list>;
DisplayListHandle MuPDFParser::get_display_list(int page_num) {
    ZoneScoped;
    fz_page* page = nullptr;
    fz_display_list* raw_list = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, doc, page_num);
    }
    fz_catch(ctx) {
        std::cerr << "Failed to load page" << std::endl;
        return nullptr;
    }
    fz_try(ctx) {
        raw_list = fz_new_display_list_from_page(ctx, page);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        if (page) {
            fz_drop_page(ctx, page);
        }
        std::cerr << "Failed to create display list" << std::endl;
        return nullptr; // coordinator will check if displaylist was created successfully
    }
    fz_context* captured_ctx = this->ctx; // capture for custom deleter
    return DisplayListHandle(raw_list, [captured_ctx](fz_display_list* ptr) {
        if (ptr) {
            fz_drop_display_list(captured_ctx, ptr);
        }
    });
}

void MuPDFParser::write_section(int page_num, int w, int h, float zoom, float rotate,
                            DisplayListHandle dlist, unsigned char* buffer, fz_rect clip) {
    /* dlist is created by another thread. That thread will be responsible for dropping it
     * clip is which portion of the dlist we are reading from. it must match with w and h
     * The input buffer must be shifted such that the first pixel drawn is at its correct position
     * This allows multiple threads to write to the buffer in parallel all to different sections at once
     */
    ZoneScoped;
    if (w != clip.x1 - clip.x0 || h != clip.y1 - clip.y0) {
        std::cerr << "ERROR: clip dimensions do not match w and h." << std::endl;
        return;
    }
    fz_pixmap* pix = nullptr;
    fz_try(ctx) {
        fz_matrix ctm = fz_scale(zoom, zoom);
        ctm = fz_pre_rotate(ctm, rotate);
        pix = fz_new_pixmap_with_bbox_and_data(ctx, fz_device_rgb(ctx),
                fz_irect_from_rect(clip), NULL, 0, buffer);
        pix->x = static_cast<int>(clip.x0);
        pix->y = static_cast<int>(clip.y0);
        fz_clear_pixmap_with_value(ctx, pix, 255); // set white background
        fz_device* dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_run_display_list(ctx, dlist.get(), dev, ctm, clip, NULL);

        // free memory
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
        std::cerr <<"ERROR: Failed to draw page." << std::endl;
    }
}

std::unique_ptr<Parser> MuPDFParser::duplicate() const {

    fz_context* clone_ctx = fz_clone_context(ctx);
    auto new_parser = std::make_unique<MuPDFParser>(this -> use_icc_profile, clone_ctx); // debug try without clone

    if (!full_filepath.empty()) {
        if (!new_parser->load_document(this->full_filepath)) {
            throw std::runtime_error("Failed to load document for cloned parser");
        }
    }
    return new_parser;
}



MuPDFParser::MuPDFParser(MuPDFParser&& other) noexcept {
    ctx = other.ctx;
    doc = other.doc;
    doc_name = other.doc_name;
    use_icc_profile = other.use_icc_profile;

    other.ctx = nullptr;
    other.doc = nullptr;
    other.doc_name.clear();
    other.use_icc_profile = false;
}

MuPDFParser& MuPDFParser::operator=(MuPDFParser&& other) noexcept {
    if (this != &other) {
        // clear current processes
        clear_doc();
        if (ctx) {
            fz_drop_context(ctx);
        }

        ctx = other.ctx;
        doc = other.doc;
        doc_name = other.doc_name;
        use_icc_profile = other.use_icc_profile;

        other.ctx = nullptr;
        other.doc = nullptr;
        other.doc_name.clear();
        other.use_icc_profile = false;
    }
    return *this;
}