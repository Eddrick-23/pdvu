#include "viewer.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <sys/poll.h>

#include "terminal.h"
#include "parser.h"
#include "renderer.h"
#include "shm.h"
#include "ram_usage.h"
// enum Key {
//    ARROW_LEFT = 1000,
//    ARROW_RIGHT,
//    ARROW_UP,
//    ARROW_DOWN
// };

Viewer::Viewer(const std::string& file_path, const bool use_ICC) : term{}, parser {use_ICC}{
   setup(file_path, use_ICC);
}

void Viewer::setup(const std::string& file_path, bool use_ICC) {
   // setup terminal and parser engines
   if (!parser.load_document(file_path)) {
      throw std::runtime_error("failed to load document");
   }
   total_pages = parser.num_pages();
   shm_supported = is_shm_supported();
}

float Viewer::calculate_zoom_factor(const TermSize& ts, int page_num, int ppr, int ppc) {
   //calculate required zoom factor to render image
   int available_rows = ts.height - 2; // top and bottom bar
   int available_cols = ts.width;

   // calculate horizontal scale
   int max_h_pixels = available_rows * ppr;
   int max_w_pixels = available_cols * ppc;

   PageSpecs ps = parser.page_specs(page_num, 1.0); // default zoom

   const float h_scale = static_cast<float>(max_w_pixels) / ps.width;
   const float v_scale = static_cast<float>(max_h_pixels) / ps.height;

   return std::min(h_scale, v_scale);
}

std::string Viewer::center_cursor(int w, int h, int ppr, int ppc, int rows, int cols,
                           int start_row, int start_col) {
   /* move cursor to appropriate position before rendering image
   so that final image is centered */

   const int cols_used = static_cast<int>(std::ceil(static_cast<float>(w) / ppc));
   const int rows_used = static_cast<int>(std::ceil(static_cast<float>(h) / ppr));

   int top_margin = (rows - rows_used) / 2;
   int left_margin = (cols - cols_used) / 2;

   top_margin = top_margin > 0 ? top_margin : 0;
   left_margin = left_margin > 0 ? left_margin : 0;
   return term.move_cursor(top_margin + start_row, left_margin + start_col);
}

void Viewer::render_page(int page_num) {
   auto start = std::chrono::high_resolution_clock::now();
   std::string frame_buffer = term.reset_screen_and_cursor_string(); // store sequence flush once at the end
   const TermSize ts = term.get_terminal_size();

   if (ts.height < MIN_ROWS || ts.width < MIN_COLS) { // guard for window size
      frame_buffer += guard_message(ts);
      std::cout << frame_buffer << std::flush;
      return;
   }

   const float zoom_factor = calculate_zoom_factor(ts, page_num, ts.pixels_per_row, ts.pixels_per_col);
   PageSpecs ps = parser.page_specs(page_num, zoom_factor);

   std::string image_sequence;
   if (shm_supported) {
      auto new_shm = std::make_unique<SharedMemory>(ps.size);
      current_shared_mem = std::move(new_shm);

      // write directly to shared memory buffer
      parser.write_page(page_num, ps.width, ps.height, zoom_factor, 0,
                        static_cast<unsigned char* >(current_shared_mem->data()), ps.size);
      image_sequence = get_image_sequence(current_shared_mem->name(), ps.width, ps.height, "shm");
   } else {
      auto new_temp = std::make_unique<Tempfile>(ps.size);
      // write directly to tempfile buffer
      parser.write_page(page_num, ps.width, ps.height, zoom_factor, 0,
                        static_cast<unsigned char* >(new_temp->data()), ps.size);
      current_temp_file = std::move(new_temp);
      image_sequence = get_image_sequence(current_temp_file->path(), ps.width, ps.height, "tempfile");
   }

   // centralise cursor and print image
   frame_buffer += center_cursor(ps.width, ps.height, ts.pixels_per_row, ts.pixels_per_col, ts.height - 2, ts.width, 2, 1);
   frame_buffer += image_sequence;

   // Add bottom bar
   frame_buffer += term.bottom_bar_string();

   // Add top bar
   auto end = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
   auto mem_bytes = getCurrentRSS();
   double mem_usage_mb = mem_bytes / (1024.0 * 1024.0);
   std::string stats = std::to_string(duration.count()) + "ms | " + std::format("{:.1f}MB", mem_usage_mb);
   frame_buffer += term.top_bar_string(parser.get_document_name(),
                              std::to_string(current_page + 1) + "/" + std::to_string(total_pages),
                              stats);

   std::cout << frame_buffer << std::flush; // one flush at the end
}

std::string Viewer::guard_message(const TermSize& ts) {

   // TO DO
   // modify text colouring. If width is too small colour red, else green
   // same for height

   std::string result;
   const std::string red ="\033[1;31m";
   const std::string green ="\033[1;32m";
   std::string title = "Terminal size too small";
   std::string current_dimensions = (ts.width >= MIN_COLS ? green : red) + "Width = " +  std::to_string(ts.width)
                                    + (ts.height >= MIN_ROWS ? green : red) + " Height = " +  std::to_string(ts.height);
   std::string required_dimensions = "Needed: " + std::to_string(MIN_COLS) + " x " + std::to_string(MIN_ROWS);

   // centre the text
   auto add_centered = [&](int r, const std::string& text, const int text_len) {
      int c = (ts.width - text_len) / 2;
      if (c < 1) c = 1;
      result += "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H" + text;
   };

   int center_row = ts.height / 2;

   add_centered(center_row - 2, title, title.length());
   add_centered(center_row, current_dimensions, visible_length(current_dimensions));

   result += green;
   add_centered(center_row + 2, required_dimensions, required_dimensions.length());
   result += "\033[0m";       // Reset
   return result;
}

int Viewer::visible_length(const std::string &s) {
   int count = 0;
   bool in_escape = false;
   for (size_t i = 0; i < s.length(); ++i) {
      if (!in_escape) {
         if (s[i] == '\033') {
            in_escape = true;
         } else {
            count++;
         }
      } else {
         if (s[i] == 'm') {
            in_escape = false;
         }
      }
   }
   return count;
}

void Viewer::process_keypress() {
   InputEvent input = term.read_input();

   if (input.key == key_none ) return; // handle interrupt, do nothing
   switch (input.key) {
      case key_right_arrow:
         if (current_page >= total_pages - 1) {
            current_page = total_pages - 1;
         } else {
            current_page++;
            render_page(current_page);
         }
         break;
      case key_left_arrow:
         if (current_page <= 0) current_page = 0;
         else {
            current_page--;
            render_page(current_page);
         }
         break;
      case key_char:
         if (input.char_value == 'q') { // quit
            running = false;
            break;
         }
         if (input.char_value == 'g') { // go to page
            term.get_input("Go to page: ");
            render_page(current_page);
            break;
         }
      default: // do nothing for the rest
   }
}

// void Viewer::print_terminal_details() { // keep for debugging zoom scaling in the future
//    TermSize ts = term.get_terminal_size();
//    std::cout << ts.width << "x" << ts.height << std::endl;
//    std::cout << ts.x << " " << ts.y << std::endl;
// }

void Viewer::run() {
   running = true;
   term.enter_raw_mode();
   term.enter_alt_screen();
   term.setup_resize_handler();
   term.hide_cursor();
   render_page(current_page);


   using Clock = std::chrono::steady_clock;
   auto start = Clock::now();
   bool resizing = false;
   while (running) {
      process_keypress();

      if (term.was_resized()) {
         start = Clock::now();
         resizing = true;
      }

      if (resizing) {
         auto now = Clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
         if (duration.count() > 75) { // wait 75ms from last signal
            render_page(current_page);
            resizing = false;
         }
      }
   }
}