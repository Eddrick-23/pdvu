#include "viewer.h"
#include "kitty.h"
#include "parser.h"
#include "terminal.h"
#include "tui.h"
#include "utils/ram_usage.h"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <print>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#endif
Viewer::Viewer(std::unique_ptr<pdf::Parser> main_parser,
               std::unique_ptr<RenderEngine> render_engine, bool use_shm) {
  ZoneScopedN("Viewer setup");
  renderer = std::move(render_engine);
  parser = std::move(main_parser);
  // setup(file_path, n_threads);
  total_pages = parser->num_pages();
  shm_supported = use_shm && is_shm_supported();
}

float Viewer::calculate_zoom_factor(const TermSize &ts,
                                    const pdf::PageSpecs &ps, int ppr,
                                    int ppc) {
  // calculate required zoom factor to render image
  const int available_rows = ts.height - 2; // top and bottom bar
  const int available_cols = ts.width;

  const int max_h_pixels = available_rows * ppr;
  const int max_w_pixels = available_cols * ppc;

  const float h_scale = static_cast<float>(max_w_pixels) / ps.acc_width;
  const float v_scale = static_cast<float>(max_h_pixels) / ps.acc_height;

  return std::min(h_scale, v_scale) * viewport.zoom;
}

std::string Viewer::center_cursor(int w, int h, int ppr, int ppc, int rows,
                                  int cols, int start_row, int start_col) {
  /* move cursor to appropriate position before rendering image
  so that final image is centered */

  const int cols_used =
      static_cast<int>(std::ceil(static_cast<float>(w) / ppc));
  const int rows_used =
      static_cast<int>(std::ceil(static_cast<float>(h) / ppr));

  int top_margin = (rows - rows_used) / 2;
  int left_margin = (cols - cols_used) / 2;

  top_margin = top_margin > 0 ? top_margin : 0;
  left_margin = left_margin > 0 ? left_margin : 0;
  return terminal::move_cursor(top_margin + start_row, left_margin + start_col);
}

void Viewer::render_page(int page_num) {
  // sends a request to our render engine to render the page, no blocking
  const TermSize ts = term.get_terminal_size();
  if (ts.width < TUI::MIN_COLS || ts.height < TUI::MIN_ROWS) {
    // Just update the guard text, don't bother the engine
    std::print("{}{}", terminal::reset_screen_and_cursor_string(),
               TUI::guard_message(ts));
    std::fflush(stdout);
    return;
  }
  const pdf::PageSpecs ps = parser->page_specs(page_num); // using default zoom
  const float zoom_factor =
      calculate_zoom_factor(ts, ps, ts.pixels_per_row, ts.pixels_per_col);
  renderer->request_page(page_num, zoom_factor, ps.scale(zoom_factor),
                         shm_supported ? "shm" : "tempfile");
}

void Viewer::change_zoom_index(int delta) {
  static constexpr int max_idx = zoom_levels.size() - 1;
  viewport.zoom_index = std::clamp(viewport.zoom_index + delta, 0, max_idx);
  viewport.zoom = zoom_levels[viewport.zoom_index];
}

Viewer::CropRect Viewer::calculate_crop_window() {
  // TODO should return a struct that we pass to the kitty protocol
  // get image data from latest frame, height width etc...
  // get viewport data (need to create a struct that tracks offsets and zoom
  // levels)

  // calculate the x and y off set from (0,0) where (0,0) is the top left
  // calculate the width and height to crop accordingly
  // width is terminal width, height must take into account top and bottom bar
  TermSize ts = term.get_terminal_size();
  int available_height_pixels = ts.height - 2 * ts.pixels_per_row;
  if (latest_frame.page_width <= ts.width &&
      latest_frame.page_height <= available_height_pixels) {
    // no crop needed, whole image fits in window
    return CropRect{0, 0, latest_frame.page_width, latest_frame.page_height};
  }

  int x_offset_pixels =
      std::floor(latest_frame.page_width * viewport.rel_x_offset);
  int y_offset_pixels =
      std::floor(latest_frame.page_height * viewport.rel_y_offset);

  return CropRect(x_offset_pixels, y_offset_pixels,
                  latest_frame.page_width - x_offset_pixels,
                  latest_frame.page_height - y_offset_pixels);
}
void Viewer::update_viewport(float delta_x, float delta_y) {
  // update the internal attribute viewport
  TermSize ts = term.get_terminal_size();

  // calculate size of crop window relative to image
  // TODO update ts.height to take into account top and bottom bar
  int available_height_pixels =
      ts.height - 2 * ts.pixels_per_row; // top and bottom bar
  float view_ratio_w = (float)ts.width / latest_frame.page_width;
  float view_ratio_h = (float)available_height_pix / latest_frame.page_height;

  // calculate max offsets
  float max_x_offset = std::max(0.0f, 1.0f - view_ratio_w);
  float max_y_offset = std::max(0.0f, 1.0f - view_ratio_h);

  // clamp the offsets
  viewport.rel_x_offset =
      std::clamp(viewport.rel_x_offset + delta_x, 0.0f, max_x_offset);
  viewport.rel_y_offset =
      std::clamp(viewport.rel_y_offset + delta_y, 0.0f, max_y_offset);
}

void Viewer::process_keypress() {
  InputEvent input = term.read_input(16); // 60fps

  // if guard message is being displayed, only allow q to quit
  TermSize ts = term.get_terminal_size();
  if (ts.width < TUI::MIN_COLS || ts.height < TUI::MIN_ROWS) {
    if (input.key == key_char && input.char_value == 'q') { // quit
      running = false;
      return;
    }
  }

  if (input.key == key_none)
    return; // handle interrupt, do nothing
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
    if (current_page <= 0)
      current_page = 0;
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
      handle_go_to_page();
      break;
    }
    if (input.char_value == '?') {
      handle_help_page();
      break;
    }
    if (input.char_value == 'z') { // reset zoom
      viewport.zoom_index = default_zoom_index;
      viewport.zoom = zoom_levels[default_zoom_index];
      render_page(current_page);
    }
    if (input.char_value == '=') { // zoom in
      // zoom in logic
      change_zoom_index(1);
      render_page(current_page);
      break;
    }
    if (input.char_value == '-') { // zoomout
      // zoom out logic
      change_zoom_index(-1);
      render_page(current_page);
      break;
    }
    if (input.char_value == 'w') {
      // pan up
      break;
    }
    if (input.char_value == 'a') {
      // pan left
      break;
    }
    if (input.char_value == 's') {
      // pan down
      break;
    }
    if (input.char_value == 'd') {
      // pan right
      break;
    }
  default: // do nothing for the rest
  }
}

void Viewer::handle_go_to_page() {
  bool running = true;
  bool page_change = false;
  auto is_whole_number = [](const std::string &s) {
    unsigned long value;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    // no error code + ptr reach end of string
    return result.ec == std::errc() && result.ptr == s.data() + s.size();
  };

  TermSize last_term_size = term.get_terminal_size();

  auto callback = [&]() {
    TermSize ts = term.get_terminal_size();
    // only request new page if dimensions change
    if (last_term_size.width != ts.width ||
        last_term_size.height != ts.height) {
      render_page(current_page);
      last_term_size = ts;
    }
    // if not we just check if there is a new frame to render
    if (fetch_latest_frame()) {
      display_latest_frame();
    }
  };
  while (running) {
    std::string input = TUI::bottom_input_bar(term, "Go to page: ", callback);
    if (input.empty()) {
      running = false;
    } else if (is_whole_number(input)) {
      running = false;
      int new_page = std::stoi(input) - 1; // we start counting from 1
      new_page = new_page < 0 ? 0 : new_page;
      new_page = new_page >= total_pages ? total_pages - 1 : new_page;
      page_change = new_page != current_page;
      current_page = new_page;
    }
    // else keep prompting until user presses esc
    // TODO maybe add a text to notify non string inputs?
  }
  if (page_change) {
    render_page(current_page);
  } else { // restore bottom bar only if nothing changed
    std::print("{}", TUI::bottom_bar(term.get_terminal_size()));
    std::fflush(stdout);
  }
}

void Viewer::handle_help_page() {

  auto clear_overlay = [](int start_row, int end_row, int width) {
    std::string sequence;
    sequence += clear_dim_layer();
    const std::string blank_line = std::string(width, ' ');
    for (int row = start_row; row <= end_row; row++) {
      sequence += terminal::move_cursor(row, 1);
      sequence += blank_line;
    }
    sequence += terminal::move_cursor(1, 1);
    return sequence;
  };

  TermSize last_terminal_size = term.get_terminal_size();
  std::print("{}", TUI::help_overlay(last_terminal_size));
  std::fflush(stdout);
  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  bool was_resized = false;
  bool resizing = false;

  while (true) {
    InputEvent input = term.read_input(50);
    if (term.was_resized()) {
      last_terminal_size = term.get_terminal_size();
      start = Clock::now();
      resizing = true;
    }
    if (resizing) {
      auto now = Clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > 75) {
        was_resized = true;
        std::print("{}", clear_overlay(1, last_terminal_size.height,
                                       last_terminal_size.width));
        std::fflush(stdout);
        last_terminal_size = term.get_terminal_size();
        std::print("{}", TUI::help_overlay(last_terminal_size));
        std::fflush(stdout);
        resizing = false;
      }
    }
    if (input.key == key_escape &&
        !(last_terminal_size.width < TUI::MIN_COLS ||
          last_terminal_size.height < TUI::MIN_ROWS)) {
      break;
    }
    if (input.key == key_char && input.char_value == 'q') { // allow quit
      running = false;
      return;
    }
  }
  if (was_resized) {
    render_page(current_page);
    return;
  }
  std::string sequence;
  sequence +=
      clear_overlay(2, last_terminal_size.height - 1, last_terminal_size.width);
  std::print("{}", sequence);
  std::fflush(stdout);
}

bool Viewer::fetch_latest_frame() {
  std::optional<RenderResult> result_opt = renderer->get_result();
  if (!result_opt) {
    return false;
  }
  auto &result = result_opt.value();
  if (result.req_id == latest_frame.req_id) { // check if frame is new
    return false;
  }
  if (!result.error_message.empty()) { // check if there was a render error
    const TermSize ts = term.get_terminal_size();
    std::print("{}",
               TUI::add_centered(ts.height / 2, ts.width, result.error_message,
                                 result.error_message.length()));
    std::fflush(stdout);
    return false;
  }
  latest_frame = std::move(result_opt.value()); // store latest frame
  return true;
}
void Viewer::display_latest_frame() {
  const TermSize ts = term.get_terminal_size();
  std::string frame = terminal::reset_screen_and_cursor_string();
  frame += center_cursor(latest_frame.page_width, latest_frame.page_height,
                         ts.pixels_per_row, ts.pixels_per_col, ts.height - 2,
                         ts.width, 2, 1);

  frame += get_image_sequence(latest_frame.path_to_data,
                              latest_frame.page_width, latest_frame.page_height,
                              shm_supported ? "shm" : "tempfile");

  frame += TUI::bottom_bar(ts);
  size_t mem_bytes = getCurrentRSS();
  double mem_usage_mb = mem_bytes / (1024.0 * 1024.0);
  std::string stats =
      std::to_string(latest_frame.render_time_ms) +
      std::format("ms {} ", TUI::symbols::box_single_line.at(179)) +
      std::format("{:.1f}MB", mem_usage_mb);
  frame +=
      TUI::top_bar(ts, parser->get_document_name(),
                   std::format("{}/{}", current_page + 1, total_pages), stats);

  std::print("{}", frame);
  std::fflush(stdout);
}

void Viewer::run() {
  running = true;
  {
    ZoneScopedN("Terminal setup");
    terminal::enter_alt_screen();
    terminal::hide_cursor();
    term.enter_raw_mode();
    term.setup_resize_handler();
  }
  term.was_resized(); // force fetch initial sizes and set flag to 0
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
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > 75) { // wait 75ms from last signal
        render_page(current_page);
        resizing = false;
      }
    }
    if (fetch_latest_frame()) {
      display_latest_frame();
    }
  }
}