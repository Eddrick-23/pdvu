#include "viewer.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <print>
#include <string_view>

#include "keys.h"
#include "plog/Log.h"
#include "render/parser.h"
#include "terminal/kitty.h"
#include "terminal/terminal.h"
#include "terminal/tui.h"
#include "utils/logging.h"
#include "utils/profiling.h"
#include "utils/ram_usage.h"
#include "utils/resize_debouncer.h"
namespace {  // utility functions and constants
// UI and timing constants
constexpr int RESIZE_DEBOUNCE_MS = 75;  // Milliseconds to wait after terminal resize
constexpr int INPUT_POLL_RATE_MS = 16;  // ~60 FPS for responsive main loop input
constexpr float PAN_STEP_RATIO = 0.1F;  // 10% of viewport shifted per pan keypress

std::string top_status_bar_with_stats(const TermSize& ts, const RenderResult& latest_frame,
                                      const std::string& doc_name, int page, int total_pages) {
  std::size_t mem_bytes = ram_usage::getCurrentRSS();
  double mem_usage_mb = static_cast<double>(mem_bytes) / (1024.0 * 1024.0);
  std::string stats = std::to_string(latest_frame.render_time_ms) +
                      std::format("ms {} ", TUI::symbols::box_single_line.at(179)) +
                      std::format("{:.1f}MB", mem_usage_mb);
  return TUI::top_status_bar(ts, doc_name, std::format("{}/{}", page + 1, total_pages), stats);
}
std::string bottom_bar(const TermSize& ts, float current_zoom_level, int rotation) {
  return TUI::bottom_status_bar(ts, current_zoom_level, rotation);
}

std::optional<int> parse_page_index(std::string_view input, int total_pages) {
  if (input.empty() || total_pages <= 0) {
    return std::nullopt;
  }
  unsigned int page_number = 0;

  const auto [ptr, error] = std::from_chars(input.data(), input.data() + input.size(), page_number);

  if (error == std::errc::result_out_of_range) {  // clamp to last page of document
    return total_pages - 1;
  }

  if (error != std::errc{} ||
      ptr != input.data() + input.size()) {  // whole string must be a number
    return std::nullopt;
  }

  const unsigned int bounded_page =
      std::clamp(page_number, 1U, static_cast<unsigned int>(total_pages));

  return static_cast<int>(bounded_page) - 1;
}
}  // namespace

Viewer::Viewer(std::unique_ptr<pdf::Parser> main_parser,
               std::unique_ptr<RenderEngine> render_engine, bool use_shm) {
  ZoneScopedN("Viewer setup");
  m_renderer = std::move(render_engine);
  m_parser = std::move(main_parser);
  // setup(file_path, n_threads);
  m_total_pages = m_parser->num_pages();
  m_shm_supported = use_shm && Shm::is_shm_supported();
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
  draw_for_current_mode();  // force draw guard message if start dimensions too small

  auto debouncer = ResizeDebouncer(RESIZE_DEBOUNCE_MS);
  while (m_running && !static_cast<bool>(Terminal::quit_requested)) {
    bool need_redraw = false;

    need_redraw |= process_keypress();
    need_redraw |= handle_resize(debouncer);

    if (!m_running) {
      break;
    }

    if (fetch_latest_frame()) {
      // store completed frames during Help, but don't redraw
      need_redraw |= m_ui_mode == UiMode::Browse;
    }

    if (need_redraw) {
      draw_for_current_mode();
    }
  }
}

bool Viewer::handle_resize(ResizeDebouncer& debouncer) {
  const ResizeState state = debouncer.poll(m_term.was_resized(), std::chrono::steady_clock::now());
  switch (state) {
    case ResizeState::Idle:
      return false;
    case ResizeState::Resizing:
      // draw_latest_frame(true, true);
      return true;
    case ResizeState::Settled:
      // keep mode independent. If help is open, render the resized page in the background
      // so it is ready when the help page closes.
      request_page_render(m_current_page);
      return true;
  }

  return false;
}

Viewer::Dimensions Viewer::available_window() {
  const TermSize ts = m_term.get_terminal_size();
  const int available_height_pixels = (ts.rows - 2) * ts.cell_pixel_height;
  const int available_width_pixels = ts.columns * ts.cell_pixel_width;

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
  if (result.req_id == m_render.latest_frame.req_id) {  // check if frame is new
    return false;
  }
  if (!result.error_message.empty()) {  // check if there was a render error
    const TermSize ts = m_term.get_terminal_size();
    const int error_message_length = static_cast<int>(std::ssize(result.error_message));
    std::print(
        "{}",
        TUI::add_centered(ts.rows / 2, ts.columns, result.error_message, error_message_length));
    std::fflush(stdout);
    return false;
  }
  m_render.latest_frame = std::move(result_opt.value());  // store latest frame
  return true;
}

std::string Viewer::latest_frame_sequence(const FrameDisplayParams& params) {
  const auto [existing_width, existing_height] = params.existing;
  const auto [target_width, target_height] = params.target;
  const TermSize ts = m_term.get_terminal_size();
  // prepare screen and cursor
  std::string sequence;
  sequence += terminal::reset_screen_and_cursor_string();
  sequence += terminal::move_cursor(2, 1);

  // 2 rows taken by top and bottom bar
  // start drawing from row 2 due to row 1 being taken by top bar.
  sequence += TUI::center_cursor(ts,
                                 target_width,
                                 target_height,
                                 {
                                     .cols = ts.columns,
                                     .rows = ts.rows - 2,
                                     .start_row = 2,
                                     .start_col = 1,
                                 });

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
  if (m_render.latest_frame.page_width != target_width ||
      m_render.latest_frame.page_height != target_height) {
    const float scale_factor_x =
        static_cast<float>(target_width) / static_cast<float>(existing_width);
    const float scale_factor_y =
        static_cast<float>(target_height) / static_cast<float>(existing_height);
    x_offset_pixels = static_cast<int>(static_cast<float>(x_offset_pixels) / scale_factor_x);
    crop_width = static_cast<int>(static_cast<float>(crop_width) / scale_factor_x);
    y_offset_pixels = static_cast<int>(static_cast<float>(y_offset_pixels) / scale_factor_y);
    crop_height = static_cast<int>(static_cast<float>(crop_height) / scale_factor_y);
  }

  const bool need_transmit = m_render.last_transmitted_req_id != m_render.latest_frame.req_id;
  if (need_transmit) {
    m_render.last_transmitted_req_id = m_render.latest_frame.req_id;
  }
  // generate sequence to display image
  const int target_rows = std::min(target_height / ts.cell_pixel_height, ts.rows - 2);
  sequence += kitty::get_image_sequence(m_render.latest_frame.path_to_data,
                                        KITTY_SLOT_ID,
                                        existing_width,
                                        existing_height,
                                        x_offset_pixels,
                                        y_offset_pixels,
                                        crop_width,
                                        crop_height,
                                        m_shm_supported ? "shm" : "tempfile",
                                        need_transmit,
                                        0,
                                        target_rows);

  return sequence;
}

void Viewer::draw_latest_frame(bool with_top_bar, bool with_bottom_bar) {
  const TermSize ts = m_term.get_terminal_size();

  if (TUI::is_window_too_small(ts)) {
    std::print("{}", TUI::guard_message(ts));
    std::fflush(stdout);
    return;
  }

  std::string sequence = latest_frame_sequence({
      .existing =
          {
              .width = m_render.latest_frame.page_width,
              .height = m_render.latest_frame.page_height,
          },
      .target =
          {
              .width = m_render.target_page_specs.width,
              .height = m_render.target_page_specs.height,
          },
  });

  if (with_top_bar) {
    sequence += top_status_bar_with_stats(
        ts, m_render.latest_frame, m_parser->get_document_name(), m_current_page, m_total_pages);
  }

  if (with_bottom_bar) {
    sequence += bottom_bar(ts, m_page_view.current_zoom(), m_rotation_degrees);
  }

  // flush and display
  std::print("{}", sequence);
  std::fflush(stdout);
}

void Viewer::request_page_render(int page_num) {
  const TermSize ts = m_term.get_terminal_size();
  if (TUI::is_window_too_small(ts)) {
    return;  // draw_latest_frame handles showing of the guard message
  }
  if (const auto specs = m_parser->page_specs(page_num)) {
    m_render.target_page_specs = specs->rotate_quarter_clockwise(m_rotation_degrees / 90);
    // ts.height - 2 due to rows taken by top and bottom bar
    const float zoom_factor = TUI::calculate_zoom_factor(ts,
                                                         m_render.target_page_specs,
                                                         {
                                                             .cols = ts.columns,
                                                             .rows = ts.rows - 2,
                                                         },
                                                         m_page_view.current_zoom());
    m_render.target_page_specs = m_render.target_page_specs.scale(zoom_factor);
    m_renderer->request_page(
        page_num, zoom_factor, m_render.target_page_specs, m_shm_supported ? "shm" : "tempfile");
  }
}

bool Viewer::handle_page_pan(char key) {
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
      break;
    default:  // do nothing for the rest
      break;
  }

  return viewport_changed;
}

void Viewer::handle_go_to_page() {
  // caller is in charge of restoring modes and redrawing
  // we only handle redraws for resize events while mode is active
  bool running = true;
  bool page_change = false;

  auto on_idle = [&]() {
    if (fetch_latest_frame()) {
      draw_for_current_mode();
    }
  };

  auto on_resize_settled = [&]() {
    draw_for_current_mode();
    request_page_render(m_current_page);
  };

  const TUI::InputBarDeps deps{
      .window_dimensions = [this]() { return m_term.get_terminal_size(); },
      .was_resized = [this]() { return m_term.was_resized(); },
      .read_input = [this](int timeout_ms) { return m_term.read_input(timeout_ms); },
      .on_idle = on_idle,
      .on_resize_settled = on_resize_settled,
      .debounce_ms = RESIZE_DEBOUNCE_MS,
  };

  std::string error_prompt;
  while (running) {
    const auto [input, cancelled, quit_requested] =
        TUI::bottom_input_bar("GO TO PAGE: ", deps, error_prompt);
    error_prompt.clear();
    if (quit_requested) {
      m_running = false;  // quit viewer entirely
      running = false;
    } else if (cancelled || input.empty()) {
      running = false;
    } else if (auto new_page = parse_page_index(input, m_total_pages)) {
      running = false;
      page_change = *new_page != m_current_page;
      m_current_page = *new_page;
    } else {
      error_prompt = "INVALID PAGE: ";
    }
  }
  if (page_change) {
    request_page_render(m_current_page);
  }
}

bool Viewer::handle_help_input(const InputEvent& event) {
  if (m_ui_mode != UiMode::Help) {
    PLOG_ERROR << "handle_help_input called when ui_mode is not Help";
    return false;
  }

  if (event.key == key_char && event.char_value == 'q') {
    m_running = false;
    return true;
  }

  if (event.key == key_escape && !TUI::is_window_too_small(m_term.get_terminal_size())) {
    m_ui_mode = UiMode::Browse;
    // Clear the dim layer here, or schedule as part of a subsequent browse draw?
    std::print("{}", kitty::clear_dim_layer());
    return true;
  }
  return false;
}

bool Viewer::handle_browse_input(const InputEvent& event) {
  static constexpr std::string_view pan_keys = "wWaAsSdD";
  if (m_ui_mode != UiMode::Browse) {
    PLOG_ERROR << "handle_browse_input being called when ui_mode is not browse";
    return false;
  }

  auto [key, char_value] = event;
  if (key == key_none) {
    return false;
  }

  if (TUI::is_window_too_small(m_term.get_terminal_size())) {
    if (key == key_char && char_value == 'q') {  // quit
      m_running = false;
      return m_running;
    }
    return false;
  }

  switch (key) {
    case key_right_arrow:
      if (m_current_page >= m_total_pages - 1) {
        m_current_page = m_total_pages - 1;
      } else {
        m_current_page++;
        request_page_render(m_current_page);
        return true;
      }
      return false;
    case key_left_arrow:
      if (m_current_page <= 0) {
        m_current_page = 0;
      } else {
        m_current_page--;
        request_page_render(m_current_page);
        return true;
      }
      return false;
    case key_char:
      if (char_value == 'q') {  // quit
        m_running = false;
        return false;
      }
      if (char_value == '?') {
        m_ui_mode = UiMode::Help;
        return true;
      }
      if (char_value == 'g') {
        // go to page
        m_ui_mode = UiMode::GoToPage;
        handle_go_to_page();
        m_ui_mode = UiMode::Browse;

        // redraw through main loop
        return m_running;
      }
      if (char_value == 'z') {  // reset zoom and crop offsets
        if (m_page_view.current_zoom() != 1.0) {
          m_page_view.reset_offsets_to_default();
          m_page_view.reset_zoom_to_default();
          request_page_render(m_current_page);
          return true;
        }
        return false;
      }
      if (char_value == '=' || char_value == '+') {  // zoom in
        if (m_page_view.change_zoom_index(1)) {
          request_page_render(m_current_page);
          return true;
        }
        return false;
      }
      if (char_value == '-' || char_value == '_') {  // zoom out
        if (m_page_view.change_zoom_index(-1)) {
          request_page_render(m_current_page);
          return true;
        }
        return false;
      }
      if (char_value == 'r') {
        m_rotation_degrees = (m_rotation_degrees + 90) % 360;
        request_page_render(m_current_page);
        return false;
      }
      if (pan_keys.contains(char_value)) {  // handle panning
        return handle_page_pan(char_value);
      }
    default:  // do nothing for the rest
      return false;
  }
}

void Viewer::draw_for_current_mode() {
  switch (m_ui_mode) {
    case UiMode::Browse:
      draw_latest_frame(true, true);
      break;
    case UiMode::GoToPage:
      draw_latest_frame(true, false);
      break;
    case UiMode::Help:
      std::string sequence;
      sequence += terminal::reset_screen_and_cursor_string();
      sequence += kitty::clear_dim_layer();
      sequence += TUI::help_overlay(m_term.get_terminal_size());

      std::print("{}", sequence);
      std::fflush(stdout);
      break;
  }
}

bool Viewer::process_keypress() {
  const auto event = m_term.read_input(INPUT_POLL_RATE_MS);  // 60fps
  switch (m_ui_mode) {
    case UiMode::Browse:
      return handle_browse_input(event);
    case UiMode::Help:
      return handle_help_input(event);
    case UiMode::GoToPage:
      // currently handled by tui
      return false;
  }
  return false;
}