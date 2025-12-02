#include "terminal.h"
#include "parser.h"

class Viewer {
public:
explicit Viewer(const std::string& file_path); // constructor
void setup(const std::string& file_path);
float calculate_zoom_factor(const TermSize& ts, int page_num);
void center_cursor(const TermSize& ts,const RawImage& image);
void render_page(const int page_num);
void run(); // main loop
private:
    // sub systems
    Terminal term; // terminal data and raw mode
    Parser parser; // parsing pdfs

    // current state
    int total_pages = 0;
    float zoom = 100;
    bool running = false;
};