#pragma once
#include "terminal.h"
#include "parser.h"
#include "tempfile.h"
#include "shm.h"

// struct RenderRequest {
//     int page_num;
//     int width;
//     int height;
//     float zoom;
//     // We use a generation ID to ignore results from old requests if needed
//     size_t req_id;
// };
//
// struct RenderResult {
//     size_t req_id;
//     int page_num;
//     std::string image_sequence;
//     std::string status_bar_stats;
//
//     // This will be moved to the viewer class attributes in the main thread
//     std::unique_ptr<SharedMemory> shm;
//     std::unique_ptr<Tempfile> temp;
// };


class Viewer {
public:
Viewer(const std::string& file_path, bool use_ICC); // constructor
void setup(const std::string& file_path, bool use_ICC);
int read_key();
void process_keypress();
float calculate_zoom_factor(const TermSize& ts, int page_num, int ppr, int ppc);
std::string center_cursor(int w, int h, int ppr, int ppc,
                    int rows, int cols, int start_row, int start_col);
void render_page(int page_num);
void handle_go_to_page();
void handle_help_page();
void run(); // main loop

private:
    // sub systems
    Terminal term; // terminal data and raw mode
    Parser parser; // parsing pdfs

    // current state
    std::unique_ptr<Tempfile> current_temp_file = nullptr;
    std::unique_ptr<SharedMemory> current_shared_mem = nullptr;
    int current_page = 0;
    int total_pages = 0;
    float zoom = 100;
    bool running = false;
    bool shm_supported = false;


};