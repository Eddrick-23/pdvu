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
      need_redraw |= m_ui_mode == UiMode::Browse || m_ui_mode == UiMode::GoToPage;
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

bool Viewer::is_preview_compatible() const {
  const auto& displayed = m_render.latest_frame;
  const auto& target = m_render.target_state;

  return displayed.page_num == target.page_num &&
         displayed.rendered_page_specs.rotation == target.page_specs.rotation;
}

bool Viewer::fetch_latest_frame() {
  std::optional<RenderResult> result_opt = m_renderer->get_result();
  if (!result_opt) {
    return false;
  }
  auto& result = result_opt.value();
  // ignore results superseded by a newer render request
  if (result.req_id != m_render.target_state.req_id) {
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

Viewer::FrameLayout Viewer::calculate_frame_layout(geometry::PixelSize source,
                                                   geometry::PixelSize target,
                                                   geometry::PixelRect target_crop,
                                                   const TermSize& ts,
                                                   const TUI::ContentArea& content_area) const {
  auto round_to_nearest_cell = [](int pixels, int pixels_per_cell, int max_cells) {
    return std::clamp(static_cast<int>(std::lround(static_cast<double>(pixels) /
                                                   static_cast<double>(pixels_per_cell))),
                      1,
                      max_cells);
  };
  const int placement_cols =
      round_to_nearest_cell(target.width, ts.cell_pixel_width, content_area.cols);
  const int placement_rows =
      round_to_nearest_cell(target.height, ts.cell_pixel_height, content_area.rows);

  auto [start_row, start_col] =
      TUI::centered_cursor_position(ts, target.width, target.height, content_area);
  FrameLayout result{
      .target_crop_rect = target_crop,
      .source_crop_rect = target_crop,
      .placement_origin = {.row = start_row, .col = start_col},
      .placement_cols = placement_cols,
      .placement_rows = placement_rows,
      .is_frame_native = true,
  };

  // scale crop window to fit existing bitmap if dimensions mismatch
  if (source.width != target.width || source.height != target.height) {
    const float scale_factor_x =
        static_cast<float>(target.width) / static_cast<float>(source.width);
    const float scale_factor_y =
        static_cast<float>(target.height) / static_cast<float>(source.height);
    result.source_crop_rect.x =
        static_cast<int>(static_cast<float>(target_crop.x) / scale_factor_x);
    result.source_crop_rect.y =
        static_cast<int>(static_cast<float>(target_crop.y) / scale_factor_y);
    result.source_crop_rect.width =
        static_cast<int>(static_cast<float>(target_crop.width) / scale_factor_x);
    result.source_crop_rect.height =
        static_cast<int>(static_cast<float>(target_crop.height) / scale_factor_y);
    result.is_frame_native = false;
  }

  return result;
}

std::string Viewer::latest_frame_sequence(const FrameDisplayParams& params) {
  constexpr int KITTY_SLOT_ID = 1;
  const auto [existing_width, existing_height] = params.existing;
  const auto [target_width, target_height] = params.target;
  const TermSize ts = m_term.get_terminal_size();
  // prepare screen and cursor
  std::string sequence;
  sequence += terminal::reset_screen_and_cursor_string();
  sequence += terminal::move_cursor(2, 1);

  // 2 rows taken by top and bottom bar
  // start drawing from row 2 due to row 1 being taken by top bar.
  const TUI::ContentArea area = {
      .cols = ts.columns,
      .rows = ts.rows - 2,
      .start_row = 2,
      .start_col = 1,
  };
  const auto [width, height] = available_window();
  const auto target_crop_window = m_page_view.calculate_crop_window(target_width,
                                                                    target_height,
                                                                    {
                                                                        .max_width_pixels = width,
                                                                        .max_height_pixels = height,
                                                                    });
  const auto& source_specs = m_render.latest_frame.rendered_page_specs;
  const auto& target_specs = m_render.target_state.page_specs;
  const auto frame_layout = calculate_frame_layout(
      {
          .width = source_specs.width,
          .height = source_specs.height,
      },
      {
          .width = target_specs.width,
          .height = target_specs.height,
      },
      target_crop_window,
      m_term.get_terminal_size(),
      area);

  const bool need_transmit = m_render.last_transmitted_req_id != m_render.latest_frame.req_id;
  if (need_transmit) {
    m_render.last_transmitted_req_id = m_render.latest_frame.req_id;
  }
  // generate sequence to display image
  sequence +=
      terminal::move_cursor(frame_layout.placement_origin.row, frame_layout.placement_origin.col);

  // kitty allows us to pin dimensions to cell count
  // we only use it for preview displays not native bitmaps
  int pin_cols = 0;  // no vertical bars so no need to pin
  int pin_rows = 0;  // pin due to top/bottom bars
  if (!frame_layout.is_frame_native) {
    pin_rows = frame_layout.placement_rows;
  }
  sequence += kitty::get_image_sequence(m_render.latest_frame.path_to_data,
                                        KITTY_SLOT_ID,
                                        existing_width,
                                        existing_height,
                                        frame_layout.source_crop_rect.x,
                                        frame_layout.source_crop_rect.y,
                                        frame_layout.source_crop_rect.width,
                                        frame_layout.source_crop_rect.height,
                                        m_shm_supported ? "shm" : "tempfile",
                                        need_transmit,
                                        pin_cols,
                                        pin_rows);

  return sequence;
}

void Viewer::draw_latest_frame(bool with_top_bar, bool with_bottom_bar) {
  const TermSize ts = m_term.get_terminal_size();

  if (TUI::is_window_too_small(ts)) {
    std::print("{}", TUI::guard_message(ts));
    std::fflush(stdout);
    return;
  }

  const bool preview_pending = m_render.latest_frame.req_id != m_render.target_state.req_id;
  if (preview_pending && !is_preview_compatible()) {
    return;  // Keep existing frame until new frame arrives.
  }

  const auto& source_specs = m_render.latest_frame.rendered_page_specs;
  const auto& target_specs = m_render.target_state.page_specs;
  std::string sequence = latest_frame_sequence({
      .existing =
          {
              .width = source_specs.width,
              .height = source_specs.height,
          },
      .target =
          {
              .width = target_specs.width,
              .height = target_specs.height,
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
    auto target_specs = specs->rotate_quarter_clockwise(m_rotation_degrees / 90);
    // ts.height - 2 due to rows taken by top and bottom bar
    const float zoom_factor = TUI::calculate_zoom_factor(ts,
                                                         target_specs,
                                                         {
                                                             .cols = ts.columns,
                                                             .rows = ts.rows - 2,
                                                         },
                                                         m_page_view.current_zoom());
    target_specs = target_specs.scale(zoom_factor);
    const std::size_t req_id = m_renderer->request_page(
        page_num, zoom_factor, target_specs, m_shm_supported ? "shm" : "tempfile");
    m_render.target_state = {
        .req_id = req_id,
        .page_num = page_num,
        .page_specs = target_specs,
    };
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

bool Viewer::handle_go_to_page_input(const InputEvent& event) {
  if (TUI::is_window_too_small(m_term.get_terminal_size())) {
    if (event.key == key_char && event.char_value == 'q') {
      m_running = false;
    }
    return false;
  }

  switch (m_go_to_page.input.handle(event)) {
    case TUI::InputBar::Action::None:
      return false;
    case TUI::InputBar::Action::Changed:
      m_go_to_page.input.clear_error();
      return true;
    case TUI::InputBar::Action::Cancelled:
      m_ui_mode = UiMode::Browse;
      m_go_to_page.reset();
      terminal::hide_cursor();
      return true;
    case TUI::InputBar::Action::Submitted:
      if (m_go_to_page.input.value().empty()) {  // do nothing on empty inputs
        m_go_to_page.reset();
        m_ui_mode = UiMode::Browse;
        terminal::hide_cursor();
        return true;
      }

      // non empty inputs, validate first
      const auto page = parse_page_index(m_go_to_page.input.value(), m_total_pages);
      if (!page) {             // invalid input, set error and re-prompt
        m_go_to_page.reset();  // clear any existing inputs
        m_go_to_page.input.set_error("INVALID PAGE: ");
        return true;
      }

      // valid input check for page change and request new frame if needed
      const bool page_changed = *page != m_current_page;
      m_current_page = *page;
      m_ui_mode = UiMode::Browse;

      if (page_changed) {
        request_page_render(m_current_page);
      }

      m_go_to_page.reset();
      terminal::hide_cursor();
      return true;
  }

  return false;
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
        m_go_to_page.reset();
        m_ui_mode = UiMode::GoToPage;
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
      return false;  // any un-supported key return false;
    default:         // do nothing for the rest
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
      if (!TUI::is_window_too_small(m_term.get_terminal_size())) {
        terminal::show_cursor();
        std::print("{}", m_go_to_page.input.render_sequence(m_term.get_terminal_size()));
        std::fflush(stdout);
      }
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
      return handle_go_to_page_input(event);
  }
  return false;
}