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
  return TUI::top_status_bar(
      ts, doc_name, std::format("{}/{}", page + 1, total_pages), stats);
}
std::string bottom_bar(const TermSize &ts, float current_zoom_level,
                       int rotation) {
  return TUI::bottom_status_bar(ts, current_zoom_level, rotation);
}

constexpr inline bool floats_equal(float a, float b) {
  return std::fabs(a - b) < 1e-6f;
}
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

void Viewer::run() {
  running = true;
  {
    ZoneScopedN("Terminal setup");
    terminal::enter_alt_screen();
    terminal::hide_cursor();
    term.enter_raw_mode();
    term.setup_signal_handlers();
  }
  term.was_resized(); // force fetch initial sizes and set flag to 0
  request_page_render(current_page);

  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  bool resizing = false;
  while (running && !Terminal::quit_requested) {
    process_keypress();

    if (term.was_resized()) {
      start = Clock::now();
      resizing = true;
    }

    if (resizing) {
      display_latest_frame(latest_frame.page_width, latest_frame.page_height,
                           target_page_specs.width, target_page_specs.height);
      auto now = Clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > 75) { // wait 75ms from last signal
        request_page_render(current_page);
        resizing = false;
      }
    }
    if (fetch_latest_frame()) {
      display_latest_frame(latest_frame.page_width, latest_frame.page_height,
                           target_page_specs.width, target_page_specs.height);
    }
  }
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

void Viewer::display_latest_frame(int existing_width, int existing_height,
                                  int target_width, int target_height) {
  const TermSize ts = term.get_terminal_size();
  // prepare screen and cursor
  std::string frame = terminal::reset_screen_and_cursor_string();
  frame += terminal::move_cursor(2, 1);
  frame += TUI::center_cursor(ts, target_width, target_height, ts.width,
                              ts.height - 2, 2, 1);

  // Take into account cropping
  // We always crop using the target dimensions
  constexpr int KITTY_SLOT_ID = 1;
  update_viewport(0.0, 0.0); // update incase image dimensions changed
  auto [x_offset_pixels, y_offset_pixels, crop_width, crop_height] =
      calculate_crop_window(target_width, target_height);
  // if latest frame dimensions match target, don't need to scale crop
  // if latest frame dimensions don't match target, scale the crop window
  if (latest_frame.page_width != target_width ||
      latest_frame.page_height != target_height) {
    const float scale_factor_x =
        static_cast<float>(target_width) / existing_width;
    const float scale_factor_y =
        static_cast<float>(target_height) / existing_height;
    x_offset_pixels /= scale_factor_x;
    crop_width /= scale_factor_x;
    y_offset_pixels /= scale_factor_y;
    crop_height /= scale_factor_y;
  }

  bool need_transmit = last_req_id != latest_frame.req_id;
  if (need_transmit) {
    last_req_id = latest_frame.req_id;
  }
  // generate sequence to display image
  int target_rows = ts.height - 2;
  if (target_height / ts.pixels_per_row < target_rows) {
    target_rows = target_height / ts.pixels_per_row;
  }
  frame += kitty::get_image_sequence(
      latest_frame.path_to_data, KITTY_SLOT_ID, existing_width, existing_height,
      x_offset_pixels, y_offset_pixels, crop_width, crop_height,
      shm_supported ? "shm" : "tempfile", need_transmit, 0, target_rows);
  // top and bottom status bars
  frame += bottom_bar(ts, viewport.zoom, rotation_degrees);
  frame += top_status_bar_with_stats(
      ts, latest_frame, parser->get_document_name(), current_page, total_pages);
  // flush and display
  std::print("{}", frame);
  std::fflush(stdout);
}

void Viewer::request_page_render(int page_num) {
  // sends a request to our render engine to render the page, no blocking
  const TermSize ts = term.get_terminal_size();
  if (TUI::is_window_too_small(ts)) {
    std::print("{}", TUI::guard_message(ts));
    std::fflush(stdout);
    return;
  }
  if (const auto specs = parser->page_specs(page_num)) {
    target_page_specs = specs->rotate_quarter_clockwise(rotation_degrees / 90);
    // ts.height - 2 due to rows taken by top and bottom bar
    const float zoom_factor = TUI::calculate_zoom_factor(
        ts, target_page_specs, ts.width, ts.height - 2, viewport.zoom);
    target_page_specs = target_page_specs.scale(zoom_factor);
    renderer->request_page(page_num, zoom_factor, target_page_specs,
                           shm_supported ? "shm" : "tempfile");
  }
}

void Viewer::change_zoom_index(int delta) {
  static constexpr int max_idx = zoom_levels.size() - 1;
  viewport.zoom_index = std::clamp(viewport.zoom_index + delta, 0, max_idx);
  viewport.zoom = zoom_levels[viewport.zoom_index];
}

Viewer::CropRect Viewer::calculate_crop_window(int width, int height) {
  const TermSize ts = term.get_terminal_size();

  // calculate drawable bounds
  const int available_rows = ts.height - 2; // top and bottom bar
  const int available_cols = ts.width;
  const int max_h_pixels = available_rows * ts.pixels_per_row;
  const int max_w_pixels = available_cols * ts.pixels_per_col;

  // check if no crop needed
  if (width <= max_w_pixels && height <= max_h_pixels) {
    // no crop needed, whole image fits in window
    return CropRect{0, 0, width, height};
  }

  // calculate window
  const int x_offset_pixels = std::floor(width * viewport.rel_x_offset);
  const int y_offset_pixels = std::floor(height * viewport.rel_y_offset);
  const int crop_w = std::min(max_w_pixels, width - x_offset_pixels);
  const int crop_h = std::min(max_h_pixels, height - y_offset_pixels);
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

void Viewer::handle_page_pan(char key) {
  const float old_rel_x_offset = viewport.rel_x_offset;
  const float old_rel_y_offset = viewport.rel_y_offset;
  int factor = std::isupper(key) ? 2 : 1;
  key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  switch (key) {
  case 'w': // pan up
    update_viewport(0, -0.1 * factor);
    break;
  case 'a': // pan left
    update_viewport(-0.1 * factor, 0);
    break;
  case 's': // pan down
    update_viewport(0, 0.1 * factor);
    break;
  case 'd': // pan right
    update_viewport(0.1 * factor, 0);
  default: // do nothing for the rest
  }

  // only display if viewport offset changed
  if (!floats_equal(old_rel_x_offset, viewport.rel_x_offset) ||
      !floats_equal(old_rel_y_offset, viewport.rel_y_offset)) {
    display_latest_frame(latest_frame.page_width, latest_frame.page_height,
                         target_page_specs.width, target_page_specs.height);
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
    // TODO fix visual artifact here where bottom bar first shows the regular
    // one, before jumping back to the input bar
    TermSize ts = term.get_terminal_size();
    // only request new page if dimensions change
    if (last_term_size.width != ts.width ||
        last_term_size.height != ts.height) {
      display_latest_frame(latest_frame.page_width, latest_frame.page_height,
                           target_page_specs.width, target_page_specs.height);
      request_page_render(current_page);
      last_term_size = ts;
    }
    // if not we just check if there is a new frame to render
    if (fetch_latest_frame()) {
      display_latest_frame(latest_frame.page_width, latest_frame.page_height,
                           target_page_specs.width, target_page_specs.height);
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
    request_page_render(current_page);
  } else { // restore bottom bar only if nothing changed
    std::print("{}",
               bottom_bar(last_term_size, viewport.zoom, rotation_degrees));
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
        !TUI::is_window_too_small(last_terminal_size)) {
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
    request_page_render(current_page);
    std::print("{}", sequence);
    fflush(stdout);
    return;
  }
  // redraw top and bottom bar
  sequence += top_status_bar_with_stats(last_terminal_size, latest_frame,
                                        parser->get_document_name(),
                                        current_page, total_pages);
  sequence += bottom_bar(last_terminal_size, viewport.zoom, rotation_degrees);
  std::print("{}", sequence);
  std::fflush(stdout);
}

void Viewer::process_keypress() {
  static constexpr std::string_view pan_keys = "wWaAsSdD";
  auto [key, char_value] = term.read_input(16); // 60fps
  // if guard message is being displayed, only allow q to quit
  if (TUI::is_window_too_small(term.get_terminal_size())) {
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
      request_page_render(current_page);
    }
    break;
  case key_left_arrow:
    if (current_page <= 0)
      current_page = 0;
    else {
      current_page--;
      request_page_render(current_page);
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
      // TODO only rerender if there was a change in zoom
      viewport.zoom_index = default_zoom_index;
      viewport.zoom = zoom_levels[default_zoom_index];
      request_page_render(current_page);
    }
    if (char_value == '=' || char_value == '+') { // zoom in
      // zoom in logic
      int current_zoom_index = viewport.zoom_index;
      change_zoom_index(1);
      if (viewport.zoom_index != current_zoom_index) {
        const int existing_frame_width = latest_frame.page_width;
        const int existing_frame_height = latest_frame.page_height;
        request_page_render(current_page);
        display_latest_frame(existing_frame_width, existing_frame_height,
                             target_page_specs.width, target_page_specs.height);
      }
      break;
    }
    if (char_value == '-' || char_value == '_') { // zoomout
      // zoom out logic
      int current_zoom_index = viewport.zoom_index;
      change_zoom_index(-1);
      if (viewport.zoom_index != current_zoom_index) {
        const int existing_frame_width = latest_frame.page_width;
        const int existing_frame_height = latest_frame.page_height;
        request_page_render(current_page);
        display_latest_frame(existing_frame_width, existing_frame_height,
                             target_page_specs.width, target_page_specs.height);
      }
      break;
    }
    if (char_value == 'r') {
      rotation_degrees = (rotation_degrees + 90) % 360;
      request_page_render(current_page);
      break;
    }
    if (pan_keys.contains(char_value)) { // handle panning
      handle_page_pan(char_value);
    }
  default: // do nothing for the rest
  }
}
