#include "viewer.h"
#include "kitty.h"
#include "parser.h"
#include "terminal.h"
#include "tui.h"
#include "utils/logging.h"
#include "utils/profiling.h"
#include "utils/ram_usage.h"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <print>
namespace { // utility functions
std::string top_status_bar_with_stats(const TermSize &ts,
                                      const RenderResult &latest_frame,
                                      const std::string &doc_name, int page,
                                      int total_pages) {
  size_t mem_bytes = ram_usage::getCurrentRSS();
  double mem_usage_mb = mem_bytes / (1024.0 * 1024.0);
  std::string stats =
      std::to_string(latest_frame.render_time_ms) +
      std::format("ms {} ", TUI::symbols::box_single_line.at(179)) +
      std::format("{:.1f}MB", mem_usage_mb);
  return TUI::top_status_bar(ts, doc_name, std::format("{}/{}", page + 1, total_pages),
                      stats);
}
std::string bottom_bar(const TermSize &ts, float current_zoom_level) { return TUI::bottom_status_bar(ts, current_zoom_level); }
} // namespace

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
    std::print("{}", TUI::guard_message(ts));
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
  const TermSize ts = term.get_terminal_size();

  // calculate drawable bounds
  const int available_rows = ts.height - 2; // top and bottom bar
  const int available_cols = ts.width;
  const int max_h_pixels = available_rows * ts.pixels_per_row;
  const int max_w_pixels = available_cols * ts.pixels_per_col;

  // check if no crop needed
  if (latest_frame.page_width <= max_w_pixels &&
      latest_frame.page_height <= max_h_pixels) {
    // no crop needed, whole image fits in window
    return CropRect{0, 0, latest_frame.page_width, latest_frame.page_height};
  }

  // calculate window
  const int x_offset_pixels =
      std::floor(latest_frame.page_width * viewport.rel_x_offset);
  const int y_offset_pixels =
      std::floor(latest_frame.page_height * viewport.rel_y_offset);
  const int crop_w =
      std::min(max_w_pixels, latest_frame.page_width - x_offset_pixels);
  const int crop_h =
      std::min(max_h_pixels, latest_frame.page_height - y_offset_pixels);
  return CropRect(x_offset_pixels, y_offset_pixels, crop_w, crop_h);
}
void Viewer::update_viewport(float delta_x, float delta_y) {
  const TermSize ts = term.get_terminal_size();

  // calculate size of crop window relative to image
  const int available_height_pixels = (ts.height - 2) * ts.pixels_per_row;
  const float view_ratio_w = static_cast<float>(ts.x) / latest_frame.page_width;
  const float view_ratio_h =
      static_cast<float>(available_height_pixels) / latest_frame.page_height;

  // calculate max offsets
  const float max_x_offset = std::max(0.0f, 1.0f - view_ratio_w);
  const float max_y_offset = std::max(0.0f, 1.0f - view_ratio_h);

  // clamp the offsets
  viewport.rel_x_offset =
      std::clamp(viewport.rel_x_offset + delta_x, 0.0f, max_x_offset);
  viewport.rel_y_offset =
      std::clamp(viewport.rel_y_offset + delta_y, 0.0f, max_y_offset);
}

void Viewer::process_keypress() {
  auto [key, char_value] = term.read_input(16); // 60fps
  // if guard message is being displayed, only allow q to quit
  TermSize ts = term.get_terminal_size();
  if (ts.width < TUI::MIN_COLS || ts.height < TUI::MIN_ROWS) {
    if (key == key_char && char_value == 'q') { // quit
      running = false;
      return;
    }
  }

  if (key == key_none)
    return; // handle interrupt, do nothing
  switch (key) {
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
    if (char_value == 'q') { // quit
      running = false;
      break;
    }
    if (char_value == 'g') { // go to page
      handle_go_to_page();
      break;
    }
    if (char_value == '?') {
      handle_help_page();
      break;
    }
    if (char_value == 'z') { // reset zoom
      viewport.zoom_index = default_zoom_index;
      viewport.zoom = zoom_levels[default_zoom_index];
      render_page(current_page);
    }
    if (char_value == '=' || char_value == '+') { // zoom in
      // zoom in logic
      int current_zoom_index = viewport.zoom_index;
      change_zoom_index(1);
      if (viewport.zoom_index != current_zoom_index) {
        render_page(current_page);
      }
      break;
    }
    if (char_value == '-' || char_value == '_') { // zoomout
      // zoom out logic
      int current_zoom_index = viewport.zoom_index;
      change_zoom_index(-1);
      if (viewport.zoom_index != current_zoom_index) {
        render_page(current_page);
      }
      break;
    }
    if (char_value ==
        'w') { // pan up  TODO, add in capital letters to pan at double rate
      update_viewport(0, -0.1);
      display_latest_frame();
      break;
    }
    if (char_value ==
        'a') { // pan left TODO, for panning if viewport not changed, don't need
               // to display frame -> do nothing
      update_viewport(-0.1, 0);
      display_latest_frame();
      break;
    }
    if (char_value == 's') { // pan down
      update_viewport(0, 0.1);
      display_latest_frame();
      break;
    }
    if (char_value == 'd') { // pan right
      update_viewport(0.1, 0);
      display_latest_frame();
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
    std::print("{}", bottom_bar(last_term_size, viewport.zoom));
    std::fflush(stdout);
  }
}

void Viewer::handle_help_page() {

  auto clear_overlay = [](int start_row, int end_row, int width) {
    std::string sequence;
    sequence += terminal::reset_screen_and_cursor_string();
    sequence += kitty::clear_dim_layer();
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
  std::string sequence;
  sequence +=
      clear_overlay(2, last_terminal_size.height - 1, last_terminal_size.width);
  if (was_resized) {
    render_page(current_page);
    std::print("{}", sequence);
    fflush(stdout);
    return;
  }
  // redraw top and bottom bar
  sequence += top_status_bar_with_stats(last_terminal_size, latest_frame,
                                        parser->get_document_name(),
                                        current_page, total_pages);
  sequence += bottom_bar(last_terminal_size, viewport.zoom);
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
  // prepare screen and cursor
  std::string frame = terminal::reset_screen_and_cursor_string();
  frame += center_cursor(latest_frame.page_width, latest_frame.page_height,
                         ts.pixels_per_row, ts.pixels_per_col, ts.height - 2,
                         ts.width, 2, 1);

  // Take into account cropping
  constexpr int KITTY_SLOT_ID = 1;
  update_viewport(0.0, 0.0); // update incase image dimensions changed
  auto [x_offset_pixels, y_offset_pixels, width, height] =
      calculate_crop_window();
  bool need_transmit = last_req_id != latest_frame.req_id;
  if (need_transmit) {
    last_req_id = latest_frame.req_id;
  }
  // generate sequence to display image
  frame += kitty::get_image_sequence(
      latest_frame.path_to_data, KITTY_SLOT_ID, latest_frame.page_width,
      latest_frame.page_height, x_offset_pixels, y_offset_pixels, width, height,
      shm_supported ? "shm" : "tempfile", need_transmit);

  // top and bottom status bars
  frame += bottom_bar(ts, viewport.zoom);
  frame += top_status_bar_with_stats(
      ts, latest_frame, parser->get_document_name(), current_page, total_pages);
  // flush and display
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