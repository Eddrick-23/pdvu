#include "viewer.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <print>

#include "keys.h"
#include "render/parser.h"
#include "terminal/kitty.h"
#include "terminal/terminal.h"
#include "terminal/tui.h"
#include "utils/logging.h"
#include "utils/profiling.h"
#include "utils/ram_usage.h"
namespace {  // utility functions and constants
// UI and timing constants
constexpr int RESIZE_DEBOUNCE_MS = 75;  // Milliseconds to wait after terminal resize
constexpr int INPUT_POLL_RATE_MS = 16;  // ~60 FPS for responsive main loop input
constexpr int HELP_POLL_RATE_MS = 50;   // Slower poll rate when idle in help menu
constexpr float PAN_STEP_RATIO = 0.1F;  // 10% of viewport shifted per pan keypress

std::string top_status_bar_with_stats(const TermSize& ts, const RenderResult& latest_frame,
    const std::string& doc_name, int page, int total_pages) {
  size_t mem_bytes = ram_usage::getCurrentRSS();
  double mem_usage_mb = mem_bytes / (1024.0 * 1024.0);
  std::string stats = std::to_string(latest_frame.render_time_ms) +
                      std::format("ms {} ", TUI::symbols::box_single_line.at(179)) +
                      std::format("{:.1f}MB", mem_usage_mb);
  return TUI::top_status_bar(ts, doc_name, std::format("{}/{}", page + 1, total_pages), stats);
}
std::string bottom_bar(const TermSize& ts, float current_zoom_level, int rotation) {
  return TUI::bottom_status_bar(ts, current_zoom_level, rotation);
}

constexpr bool floats_equal(float a, float b) { return std::fabs(a - b) < 1e-6F; }
}  // namespace

Viewer::Viewer(std::unique_ptr<pdf::Parser> main_parser,
    std::unique_ptr<RenderEngine> render_engine, bool use_shm) {
  ZoneScopedN("Viewer setup");
  m_renderer = std::move(render_engine);
  m_parser = std::move(main_parser);
  // setup(file_path, n_threads);
  m_total_pages = m_parser->num_pages();
  m_shm_supported = use_shm && is_shm_supported();
}

void Viewer::run() {
  m_running = true;
  {
    ZoneScopedN("Terminal setup");
    terminal::enter_alt_screen();
    terminal::hide_cursor();
    m_term.enter_raw_mode();
    m_term.setup_signal_handlers();
  }
  m_term.was_resized();  // force fetch initial sizes and set flag to 0
  request_page_render(m_current_page);

  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  bool resizing = false;
  while (m_running && !Terminal::quit_requested) {
    process_keypress();

    if (m_term.was_resized()) {
      start = Clock::now();
      resizing = true;
    }

    if (resizing) {
      draw_latest_frame(true, true);
      auto now = Clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > RESIZE_DEBOUNCE_MS) {  // wait 75ms from last signal
        request_page_render(m_current_page);
        resizing = false;
      }
    }
    if (fetch_latest_frame()) {
      draw_latest_frame(true, true);
    }
  }
}

Viewer::Dimensions Viewer::available_window() {
  const TermSize ts = m_term.get_terminal_size();
  const int available_height_pixels = (ts.height - 2) * ts.pixels_per_row;
  const int available_width_pixels = ts.width * ts.pixels_per_col;

  return Dimensions{
      .width = available_width_pixels,
      .height = available_height_pixels,
  };
}

bool Viewer::fetch_latest_frame() {
  std::optional<RenderResult> result_opt = m_renderer->get_result();
  if (!result_opt) {
    return false;
  }
  auto& result = result_opt.value();
  if (result.req_id == m_latest_frame.req_id) {  // check if frame is new
    return false;
  }
  if (!result.error_message.empty()) {  // check if there was a render error
    const TermSize ts = m_term.get_terminal_size();
    std::print("{}", TUI::add_centered(ts.height / 2, ts.width, result.error_message,
                         result.error_message.length()));
    std::fflush(stdout);
    return false;
  }
  m_latest_frame = std::move(result_opt.value());  // store latest frame
  return true;
}

std::string Viewer::latest_frame_sequence(const FrameDisplayParams& params) {
  const auto [existing_width, existing_height] = params.existing;
  const auto [target_width, target_height] = params.target;
  const TermSize ts = m_term.get_terminal_size();
  // prepare screen and cursor
  std::string sequence = terminal::reset_screen_and_cursor_string();
  sequence += terminal::move_cursor(2, 1);
  sequence += TUI::center_cursor(ts, target_width, target_height, ts.width, ts.height - 2, 2, 1);

  // Take into account cropping
  // We always crop using the target dimensions
  constexpr int KITTY_SLOT_ID = 1;

  // update viewport in case image dimensions changed
  const auto [width, height] = available_window();

  auto [x_offset_pixels, y_offset_pixels, crop_width, crop_height] =
      m_page_view.calculate_crop_window(
          target_width, target_height, {.max_width_pixels = width, .max_height_pixels = height});
  // if latest frame dimensions match target, don't need to scale crop
  // if latest frame dimensions don't match target, scale the crop window
  if (m_latest_frame.page_width != target_width || m_latest_frame.page_height != target_height) {
    const float scale_factor_x = static_cast<float>(target_width) / existing_width;
    const float scale_factor_y = static_cast<float>(target_height) / existing_height;
    x_offset_pixels /= scale_factor_x;
    crop_width /= scale_factor_x;
    y_offset_pixels /= scale_factor_y;
    crop_height /= scale_factor_y;
  }

  bool need_transmit = m_last_req_id != m_latest_frame.req_id;
  if (need_transmit) {
    m_last_req_id = m_latest_frame.req_id;
  }
  // generate sequence to display image
  int target_rows = ts.height - 2;
  if (target_height / ts.pixels_per_row < target_rows) {
    target_rows = target_height / ts.pixels_per_row;
  }
  sequence += kitty::get_image_sequence(m_latest_frame.path_to_data, KITTY_SLOT_ID, existing_width,
      existing_height, x_offset_pixels, y_offset_pixels, crop_width, crop_height,
      m_shm_supported ? "shm" : "tempfile", need_transmit, 0, target_rows);

  return sequence;
}

void Viewer::draw_latest_frame(bool with_top_bar, bool with_bottom_bar) {
  TermSize ts = m_term.get_terminal_size();

  std::string sequence = latest_frame_sequence({
      .existing = {.width = m_latest_frame.page_width, .height = m_latest_frame.page_height},
      .target = {.width = m_target_page_specs.width, .height = m_target_page_specs.height},
  });

  if (with_top_bar) {
    sequence += top_status_bar_with_stats(
        ts, m_latest_frame, m_parser->get_document_name(), m_current_page, m_total_pages);
  }

  if (with_bottom_bar) {
    sequence += bottom_bar(ts, m_page_view.current_zoom(), m_rotation_degrees);
  }

  // flush and display
  std::print("{}", sequence);
  std::fflush(stdout);
}

void Viewer::request_page_render(int page_num) {
  // sends a request to our render engine to render the page, no blocking
  const TermSize ts = m_term.get_terminal_size();
  if (TUI::is_window_too_small(ts)) {
    std::print("{}", TUI::guard_message(ts));
    std::fflush(stdout);
    return;
  }
  if (const auto specs = m_parser->page_specs(page_num)) {
    m_target_page_specs = specs->rotate_quarter_clockwise(m_rotation_degrees / 90);
    // ts.height - 2 due to rows taken by top and bottom bar
    const float zoom_factor = TUI::calculate_zoom_factor(
        ts, m_target_page_specs, ts.width, ts.height - 2, m_page_view.current_zoom());
    m_target_page_specs = m_target_page_specs.scale(zoom_factor);
    m_renderer->request_page(
        page_num, zoom_factor, m_target_page_specs, m_shm_supported ? "shm" : "tempfile");
  }
}

void Viewer::handle_page_pan(char key) {
  bool viewport_changed = false;
  const float factor = (std::isupper(key) != 0) ? 2 : 1;
  key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  switch (key) {
    case 'w':  // pan up
      viewport_changed = m_page_view.update_viewport(0, -PAN_STEP_RATIO * factor);
      break;
    case 'a':  // pan left
      viewport_changed = m_page_view.update_viewport(-PAN_STEP_RATIO * factor, 0);
      break;
    case 's':  // pan down
      viewport_changed = m_page_view.update_viewport(0, PAN_STEP_RATIO * factor);
      break;
    case 'd':  // pan right
      viewport_changed = m_page_view.update_viewport(PAN_STEP_RATIO * factor, 0);
    default:  // do nothing for the rest
      break;
  }

  // only re-display if viewport offset changed
  if (viewport_changed) {
    draw_latest_frame(true, true);
  }
}

void Viewer::handle_go_to_page() {
  bool running = true;
  bool page_change = false;
  auto is_whole_number = [](const std::string& s) {
    unsigned long value;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    // no error code + ptr reach end of string
    return result.ec == std::errc() && result.ptr == s.data() + s.size();
  };

  TermSize last_term_size = m_term.get_terminal_size();

  auto callback = [&]() {
    TermSize ts = m_term.get_terminal_size();
    // only request new page if dimensions change
    if (last_term_size.width != ts.width || last_term_size.height != ts.height) {
      draw_latest_frame(true, false);
      request_page_render(m_current_page);
      last_term_size = ts;
    }
    // if not we just check if there is a new frame to render
    if (fetch_latest_frame()) {
      draw_latest_frame(true, false);
    }
  };
  while (running) {
    std::string input = TUI::bottom_input_bar(m_term, "Go to page: ", callback);
    if (input.empty()) {
      running = false;
    } else if (is_whole_number(input)) {
      running = false;
      int new_page = std::stoi(input) - 1;  // we start counting from 1
      new_page = new_page < 0 ? 0 : new_page;
      new_page = new_page >= m_total_pages ? m_total_pages - 1 : new_page;
      page_change = new_page != m_current_page;
      m_current_page = new_page;
    }
    // else keep prompting until user presses esc
    // TODO maybe add a text to notify non string inputs?
  }
  if (page_change) {
    request_page_render(m_current_page);
  } else {  // restore bottom bar only if nothing changed
    std::print("{}", bottom_bar(last_term_size, m_page_view.current_zoom(), m_rotation_degrees));
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

  TermSize last_terminal_size = m_term.get_terminal_size();
  std::print("{}", TUI::help_overlay(last_terminal_size));
  std::fflush(stdout);
  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  bool was_resized = false;
  bool resizing = false;

  while (true) {
    InputEvent input = m_term.read_input(HELP_POLL_RATE_MS);
    if (m_term.was_resized()) {
      last_terminal_size = m_term.get_terminal_size();
      start = Clock::now();
      resizing = true;
    }
    if (resizing) {
      auto now = Clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > RESIZE_DEBOUNCE_MS) {
        was_resized = true;
        std::print("{}", clear_overlay(1, last_terminal_size.height, last_terminal_size.width));
        std::fflush(stdout);
        last_terminal_size = m_term.get_terminal_size();
        std::print("{}", TUI::help_overlay(last_terminal_size));
        std::fflush(stdout);
        resizing = false;
      }
    }
    if (input.key == key_escape && !TUI::is_window_too_small(last_terminal_size)) {
      break;
    }
    if (input.key == key_char && input.char_value == 'q') {  // allow quit
      m_running = false;
      return;
    }
  }
  std::string sequence;
  sequence += clear_overlay(2, last_terminal_size.height - 1, last_terminal_size.width);
  if (was_resized) {
    request_page_render(m_current_page);
    std::print("{}", sequence);
    fflush(stdout);
    return;
  }
  // redraw top and bottom bar
  sequence += top_status_bar_with_stats(last_terminal_size, m_latest_frame,
      m_parser->get_document_name(), m_current_page, m_total_pages);
  sequence += bottom_bar(last_terminal_size, m_page_view.current_zoom(), m_rotation_degrees);
  std::print("{}", sequence);
  std::fflush(stdout);
}

void Viewer::process_keypress() {
  static constexpr std::string_view pan_keys = "wWaAsSdD";
  auto [key, char_value] = m_term.read_input(INPUT_POLL_RATE_MS);  // 60fps
  // if guard message is being displayed, only allow q to quit
  if (TUI::is_window_too_small(m_term.get_terminal_size())) {
    if (key == key_char && char_value == 'q') {  // quit
      m_running = false;
      return;
    }
  }

  if (key == key_none) return;  // handle interrupt, do nothing
  switch (key) {
    case key_right_arrow:
      if (m_current_page >= m_total_pages - 1) {
        m_current_page = m_total_pages - 1;
      } else {
        m_current_page++;
        request_page_render(m_current_page);
      }
      break;
    case key_left_arrow:
      if (m_current_page <= 0) {
        m_current_page = 0;
      } else {
        m_current_page--;
        request_page_render(m_current_page);
      }
      break;
    case key_char:
      if (char_value == 'q') {  // quit
        m_running = false;
        break;
      }
      if (char_value == 'g') {  // go to page
        handle_go_to_page();
        break;
      }
      if (char_value == '?') {
        handle_help_page();
        break;
      }
      if (char_value == 'z') {  // reset zoom
        if (m_page_view.current_zoom() != 1.0) {
          m_page_view.reset_zoom_to_default();
          request_page_render(m_current_page);
        }
      }
      if (char_value == '=' || char_value == '+') {  // zoom in
        if (m_page_view.change_zoom_index(1)) {
          request_page_render(m_current_page);
          draw_latest_frame(true, true);
        }
        break;
      }
      if (char_value == '-' || char_value == '_') {  // zoom out
        if (m_page_view.change_zoom_index(-1)) {
          request_page_render(m_current_page);
          draw_latest_frame(true, true);
        }
        break;
      }
      if (char_value == 'r') {
        m_rotation_degrees = (m_rotation_degrees + 90) % 360;
        request_page_render(m_current_page);
        break;
      }
      if (pan_keys.contains(char_value)) {  // handle panning
        handle_page_pan(char_value);
      }
    default:  // do nothing for the rest
      break;
  }
}
