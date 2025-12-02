#include <iostream>
#include "terminal.h"
#include "parser.h"
#include "renderer.h"
#include "viewer.h"
Viewer::Viewer(const std::string& file_path) {
   setup(file_path);
}

void Viewer::setup(const std::string& file_path) {
   // setup terminal and parser engines
   if (!parser.load_document(file_path)) {
      throw std::runtime_error("failed to load document");
   }
   total_pages = parser.num_pages();
}

float Viewer::calculate_zoom_factor(const TermSize& ts, const int page_num) {
   //calculate required zoom factor to render image
   PageBounds pb = parser.get_page_dimensions(page_num);

   // calculate horizontal scale
   const float h_scale = static_cast<float>(ts.x) / pb.x1;
   // calculate vertical scale
   const float v_scale = static_cast<float>(ts.y) / pb.y1;
   std::cout << h_scale << " " << v_scale << std::endl;
   return std::min(h_scale, v_scale) * 100;

}

void Viewer::center_cursor(const TermSize& ts, const RawImage& image) {
   /* move cursor to appropriate position before rendering image
   so that final image is centered */
   // find how many pixels per row
   // find how many pixels per width
   const float pixels_per_row = static_cast<float>(ts.y) / ts.height;
   const float pixels_per_col = static_cast<float>(ts.x) / ts.width;

   const int cols_used = static_cast<int>(std::ceil(static_cast<float>(image.width) / pixels_per_col));
   const int rows_used = static_cast<int>(std::ceil(static_cast<float>(image.height) / pixels_per_row));

   int top_margin = (ts.height - rows_used) / 2;
   int left_margin = (ts.width - cols_used) / 2;

   std::cout << "[DEBUG] Scaled Grid: " << ts.width << "x" << ts.height << std::endl;
   std::cout << "[DEBUG] Margin: " << left_margin << std::endl;

   top_margin = top_margin > 0 ? top_margin : 0;
   left_margin = left_margin > 0 ? left_margin : 0;

   std::cout << "\033[" << (top_margin + 1) << ";" << (left_margin + 1) << "H";
}

void Viewer::render_page(const int page_num) {
   const TermSize ts = term.get_terminal_size();
   const float zoom_factor = calculate_zoom_factor(ts, page_num);
   const RawImage image = parser.get_page(page_num, zoom_factor, 0);
   // center cursor first so that image is centered
   term.clear_terminal();
   center_cursor(ts, image);
   display_page(image);
}



// void Viewer::run() {
//
// }