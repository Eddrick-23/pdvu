#include "viewer.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include "terminal.h"
#include "parser.h"
#include "renderer.h"
#include "shm.h"
#include "ram_usage.h"
enum Key {
   ARROW_LEFT = 1000,
   ARROW_RIGHT,
   ARROW_UP,
   ARROW_DOWN
};

Viewer::Viewer(const std::string& file_path) {
   setup(file_path);
}

void Viewer::setup(const std::string& file_path) {
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

   PageBounds pb = parser.get_page_dimensions(page_num);

   const float h_scale = max_w_pixels / pb.x1;
   const float v_scale = max_h_pixels / pb.y1;

   return std::min(h_scale, v_scale) * 100;
}

std::string Viewer::center_cursor(const RawImage& image, int ppr, int ppc, int rows, int cols,
                           int start_row, int start_col) {
   /* move cursor to appropriate position before rendering image
   so that final image is centered */

   const int cols_used = static_cast<int>(std::ceil(static_cast<float>(image.width) / ppc));
   const int rows_used = static_cast<int>(std::ceil(static_cast<float>(image.height) / ppr));

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

   if (ts.height < MIN_ROWS || ts.width < MIN_COLS) {
      frame_buffer += guard_message(ts);
      std::cout << frame_buffer << std::flush;
      return;
   }
   const float zoom_factor = calculate_zoom_factor(ts, page_num, ts.pixels_per_row, ts.pixels_per_col);
   const RawImage image = parser.get_page(page_num, zoom_factor, 0);
   std::string image_sequence;
   if (shm_supported) {
      auto new_shm = std::make_unique<SharedMemory>(image.len);
      new_shm->write_data(image.pixels, image.len);
      new_shm->close_mem();
      current_shared_mem = std::move(new_shm);
      image_sequence = get_image_sequence(current_shared_mem->name(), image.width, image.height, "shm");
   } else {
      auto new_temp = std::make_unique<Tempfile>();
      new_temp->write_data(image.pixels, image.len);
      current_temp_file = std::move(new_temp);
      image_sequence = get_image_sequence(current_temp_file->path(), image.width, image.height, "tempfile");
   }

   // centralise cursor and print image
   frame_buffer += center_cursor(image, ts.pixels_per_row, ts.pixels_per_col, ts.height - 2, ts.width, 2, 1);
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
   std::string title = "Terminal size too small: ";
   std::string current_dimensions = "Width = " +  std::to_string(ts.width) + " Height = " +  std::to_string(ts.height);
   std::string required_dimensions = "Needed: " + std::to_string(MIN_COLS) + " x " + std::to_string(MIN_ROWS);

   // centre the text
   auto add_centered = [&](int r, const std::string& text) {
      int c = (ts.width - text.length()) / 2;
      if (c < 1) c = 1;
      result += "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H" + text;
   };

   // D. Draw the Warning Box (centered vertically)
   int center_row = ts.height / 2;

   // Use ANSI colors to mimic btop (Gray bg for title, etc)
   // result += "\033[1;47;30m"; // White BG, Black Text (Bold)
   result += "\033[1;31m";      // Bold, red foreground
   result += "\033[1;32m";      // Bold, green foreground
   add_centered(center_row - 2, " " + title + " ");
   // result += "\033[0m";       // Reset

   add_centered(center_row, current_dimensions);

   result += "\033[1;31m";      // Bold, red foreground
   add_centered(center_row + 2, required_dimensions);
   result += "\033[0m";       // Reset
   return result;
}

int Viewer::read_key() {
   int nread;
   char c;
   while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
      if (nread == -1 && errno != EAGAIN) {
         term.die("read");
      }
   }
   if (c == '\x1b') { // check for arrow key press
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
      if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

      if (seq[0] == '[') {
         switch (seq[1]) {
            case 'A' : return ARROW_UP;
            case 'B' : return ARROW_DOWN;
            case 'C' : return ARROW_RIGHT;
            case 'D' : return ARROW_LEFT;
         }
      }
      return '\x1b';
   }
   return c;
}

void Viewer::process_keypress() {
   int c = read_key();
   switch (c) {
      case static_cast<int>('q'):
         running = false;
         break;
      case ARROW_RIGHT:
         if (current_page >= total_pages -1) {
            current_page = total_pages - 1; // don't rerender
         } else {
            current_page++;
            render_page(current_page);
         }
         break;
      case ARROW_LEFT:
         if (current_page <= 0) {
            current_page = 0; // don't rerender
         } else {
            current_page--;
            render_page(current_page);
         }
         break;
   }

}

void Viewer::run() {
   running = true;
   term.enter_raw_mode();
   term.enter_alt_screen();
   term.hide_cursor();
   render_page(current_page);
   while (running) {
      process_keypress();
   }
}