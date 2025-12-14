#pragma once
#include "terminal.h"
#include "parser.h"
#include "tempfile.h"
#include "shm.h"
#include "render_engine.h"


class Viewer {
public:
Viewer(const std::string& file_path, bool use_ICC); // constructor
void setup(const std::string& file_path);
void process_keypress();
float calculate_zoom_factor(const TermSize& ts, int page_num, int ppr, int ppc);
std::string center_cursor(int w, int h, int ppr, int ppc,
                    int rows, int cols, int start_row, int start_col);
void render_page(int page_num);
void handle_go_to_page();
void handle_help_page();
bool fetch_latest_frame();
void display_latest_frame();
void run(); // main loop

private:
    // sub systems
    Terminal term; // terminal data and raw mode
    Parser parser; // parsing pdfs
    // hold a pointer because parser must be loaded fully before creating renderer
    std::unique_ptr<RenderEngine> renderer; // loading page frames

    // current state
    int current_page = 0;
    int total_pages = 0;
    float zoom = 100;
    bool running = false;
    bool shm_supported = false;
    RenderResult latest_frame;
};