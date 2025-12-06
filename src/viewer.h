#pragma once
#include "terminal.h"
#include "parser.h"
#include "tempfile.h"
#include "shm.h"
class Viewer {
public:
explicit Viewer(const std::string& file_path); // constructor
void setup(const std::string& file_path);
int read_key();
void process_keypress();
float calculate_zoom_factor(const TermSize& ts, int page_num, int ppr, int ppc);
std::string center_cursor(const RawImage& image, int ppr, int ppc,
                    int rows, int cols, int start_row, int start_col);
std::string guard_message(const TermSize& ts);
void render_page(int page_num);
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

    // constants
    const int MIN_ROWS = 23;
    const int MIN_COLS = 40;
};