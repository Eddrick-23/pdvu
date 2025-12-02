#include <iostream>
#include "parser.h"
#include "renderer.h"
#include "terminal.h"
#include "viewer.h"
#include <chrono>

int main() {
    std::cout << "Starting Termivue..." << std::endl;

    // Stack allocation (RAII) - cleaner than 'new'
    // Parser engine;
    // Terminal terminal;
    // engine.init_test();
    // engine.load_document("samples/introduction-to-advanced-mathematics.pdf");
    // std::cout << engine.num_pages() << std::endl;
    // std::cout << "Image details" << "\n";
    // RawImage raw = engine.get_page(0, 300, 0);
    // PageBounds d = engine.get_page_dimensions(0);
    // std::cout << d.x0 << " " << d.y0 << " " << d.x1 << " " << d.y1 << std::endl;
    // std::cout << "image height: " << raw.height << "\n";
    // std::cout << "image width: " << raw.width << "\n";
    // // std::cout << raw.len << "\n";
    // // std::cout << typeid(raw.pixels).name() << std::endl;
    // // std::cout << raw.pixels[0] << "\n";
    // // std::cout << raw.pixels[904688] << "\n";
    // // std::cout << raw.pixels[10] << "\n";
    //
    // // display_page(raw);
    // TermSize ts = terminal.get_terminal_size();
    // std::cout << "terminal height: " << ts.y << " terminal width: " << ts.x << std::endl;
    const std::string file_path = "samples/introduction-to-advanced-mathematics.pdf";
    Viewer viewer(file_path);
    // std::cout << viewer.calculate_zoom_factor(3 );
    viewer.render_page(0);
    return 0;

}
