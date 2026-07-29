#include "tui.h"

#include <cstdio>
#include <functional>
#include <print>
#include <string>
#include <string_view>

#include "ansi.h"
#include "kitty.h"
#include "terminal.h"
#include "utils/resize_debouncer.h"

namespace TUI::helplist {
static constexpr std::array<std::array<std::string_view, 2>, 15> help_text = {
    {
        {"->", "Next Page"},
        {"<-", "Previous Page"},
        {"q", "Quit"},
        {"g", "Go to Page"},
        {"Esc", "Exit input textbox"},
        {"/ or shift+f", "Find text"},  // TODO
        {"w", "Pan up"},
        {"a", "Pan left"},
        {"s", "Pan down"},
        {"d", "Pan right"},
        {"r", "Rotate clockwise 90 degrees"},
        {"+ or =", "Zoom in"},
        {"- or _", "Zoom out"},
        {"z", "Zoom to fit and reset viewport"},
        {"?", "Help page"},
    },
};
}

namespace {  // utility function
/**
 * @brief Counts how many terminal columns a string will occupy once
 * printed: total bytes, minus ANSI CSI escape sequences, minus UTF-8
 * continuation bytes (so multi-byte characters count once, not per-byte).
 *
 * Recognises escape sequences only in CSI form (<code>ESC [ ... &lt;final
 * byte&gt;</code>, e.g. colour codes, cursor moves - everything this
 * codebase emits), ending the sequence at the first byte in the 0x40-0x7E
 * range that isn't <code>[</code>, per the standard CSI terminator format.
 * Other escape sequence families (e.g. OSC, used for terminal hyperlinks or
 * title-setting) are not recognised and will be miscounted as visible
 * characters if ever passed in.
 *
 * UTF-8 continuation bytes are identified by the top two bits being
 * <code>10</code> (bit pattern <code>10xxxxxx</code>, i.e. <code>(byte &
 * 0xC0) == 0x80</code>) - true for every byte of a multi-byte character
 * except its first.
 *
 * This is a byte-width count, not a display-width count: double-width
 * Unicode characters (e.g. CJK glyphs, some emoji) occupy two terminal
 * columns but are counted here as one, which is a source of misalignment
 * anywhere this feeds into layout math (centering, padding, truncation) if
 * such characters are ever passed in.
 *
 * @param s String to measure, potentially containing CSI escape sequences
 *          and/or multi-byte UTF-8 characters.
 * @return Number of columns s would occupy once printed with its escape
 *         sequences applied.
 */
int visible_length(const std::string& s) {
  /* count visible length of string ignoring escape characters*/
  int count = 0;
  bool in_escape = false;
  for (size_t i = 0; i < s.length(); ++i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c == '\x1b') {  // start of escape character
      in_escape = true;
      continue;
    }
    if (in_escape) {
      // look for end of ANSI sequences
      // E.g. m, H, J, K, f ...
      if (c != '[' && c >= 0x40 && c <= 0x7E) {
        in_escape = false;
      }
      continue;
    }

    if ((c & 0xC0) == 0x80) {  // UTF-8 continuation byte, already counted as part of its character
      continue;
    }
    count++;  // stand char or start of a utf-8 char
  }
  return count;
}

/**
 * @brief Centres text within a fixed-width field by padding both sides with
 * spaces.
 *
 * If text is longer than width, no padding is added on either side and the
 * text is returned unchanged (text is never truncated).
 *
 * @param width Total width of the returned string, in characters - assuming
 *              text is no longer than width. Interpreted as a hard target,
 *              not a minimum: padding is split as evenly as possible between
 *              both sides, with any odd leftover space going to the right.
 * @param text Text to centre. Not truncated if longer than width.
 * @return A string of length max(width, text.length()): text with spaces
 *         added on both sides so it sits centred within width columns.
 */
std::string centre_with_space(int width, std::string_view text) {
  const int text_len = static_cast<int>(std::ssize(text));
  const int left_padding = std::max(0, (width - text_len) / 2);
  const int right_padding = std::max(0, width - text_len - left_padding);
  return std::string(left_padding, ' ') + std::string(text) + std::string(right_padding, ' ');
}

/**
 * @brief Returns a copy of str with any leading/trailing characters found in
 * whitespace removed.
 *
 * @param str Source string to trim. Left unmodified; a new string is
 *            returned rather than trimming in place.
 * @param whitespace Set of characters treated as trimmable, checked
 *                    individually (not as a substring) - e.g. the default
 *                    " \t" strips any leading/trailing run of spaces and
 *                    tabs, in any mixture.
 * @return A copy of str with leading/trailing characters from whitespace
 *         removed. Returns an empty string if str is empty or consists
 *         entirely of characters found in whitespace.
 */
std::string trim(std::string_view str, const std::string& whitespace = " \t") {
  const auto strBegin = str.find_first_not_of(whitespace);
  if (strBegin == std::string::npos) return "";  // no content

  const auto strEnd = str.find_last_not_of(whitespace);
  const auto strRange = strEnd - strBegin + 1;

  return std::string(str.substr(strBegin, strRange));
}

/**
 * @brief Rectangular bounds for create_box(), in terminal row/column space
 * (1-indexed, matching terminal::move_cursor()).
 */
struct BoxBounds {
  int start_row;  ///< Row of the box's top edge.
  int start_col;  ///< Column of the box's left edge.
  int width;      ///< Total box width in columns, including both side borders.
  int height;     ///< Total box height in rows, including both top/bottom borders.
};

/**
 * @brief Builds the escape-sequence string that draws a double-line-bordered
 * box outline (optionally with a filled/erased interior).
 *
 * @param box_bounds Position and size of the box - see BoxBounds.
 * @param fill If true, the interior is cleared to a solid background colour
 *             and the background colour is reset again at the end of the
 *             returned sequence. If false, the interior is left untouched
 *             (only the border is drawn) and the cursor is moved across each
 *             row rather than having spaces printed.
 * @return Escape sequence that draws the box when printed; does not move the
 *         cursor to any particular place afterwards beyond the last
 *         character drawn.
 */
std::string create_box(const BoxBounds& box_bounds, bool fill) {
  auto [start_row, start_col, width, height] = box_bounds;
  std::string result = fill ? std::format("\x1b[48;5;{}m", 16) : "";
  result += terminal::move_cursor(start_row, start_col);
  result += TUI::symbols::box_double_line.at(201);
  for (int i = 0; i < width - 2; i++) {
    result += TUI::symbols::box_single_line.at(196);
  }
  result += TUI::symbols::box_double_line.at(187);

  for (int i = start_row + 1; i < start_row + height; i++) {  // draw the sides
    result += terminal::move_cursor(i, start_col);
    result += TUI::symbols::box_single_line.at(179);
    if (fill) {
      result += std::string(width - 2, ' ');
    } else {
      result += terminal::move_cursor(i, start_col + width - 1);
    }
    result += TUI::symbols::box_single_line.at(179);
  }
  result += terminal::move_cursor(start_row + height, start_col);
  result += TUI::symbols::box_double_line.at(200);
  for (int i = 0; i < width - 2; i++) {
    result += TUI::symbols::box_single_line.at(196);
  }
  result += TUI::symbols::box_double_line.at(188);
  result += fill ? "\x1b[49m" : "";  // reset bg colour
  return result;
}

/**
 * @brief Computes the leftmost buffer index that should be drawn on screen
 * this frame, given where the cursor currently sits.
 *
 * The input bar tracks two independent positions into the same buffer: one
 * for where edits happen, one for where drawing starts. This is a
 * two-pointer scroll - the cursor is free to move anywhere in the buffer as
 * the user types, while the visible window only moves the minimum amount
 * needed to keep the cursor inside it, staying put on every redraw where the
 * cursor hasn't walked off either edge since the previous frame.
 *
 * @param cursor_pos Index of the cursor within the buffer, i.e. the position
 *                    the next inserted/deleted character will act on.
 * @param visible_pos Leftmost buffer index shown on screen as of the
 *                     <i>previous</i> redraw. Passed in so this call can
 *                     scroll relative to where the window already was,
 *                     rather than re-centering every frame.
 * @param buffer_len Total length of the buffer being edited (not just the
 *                    visible slice).
 * @param available_width Columns on screen available to draw the buffer in,
 *                         i.e. terminal width minus the prompt's width.
 * @return The buffer index that should become the new visible_pos: enough to
 *         keep the cursor on screen, or - once the buffer overflows the
 *         width - the index that shows its tail, so text keeps appearing to
 *         scroll left as the user types past the right edge.
 */
int scroll_window_start(int cursor_pos, int visible_pos, int buffer_len, int available_width) {
  if (cursor_pos < visible_pos) {  // scrolled left off screen
    visible_pos = cursor_pos;
  } else if (cursor_pos >= visible_pos + available_width) {  // scrolled right off screen
    visible_pos = cursor_pos - available_width + 1;
  }
  if (buffer_len > available_width) {
    visible_pos = buffer_len - available_width;
  }
  return visible_pos;
}
}  // namespace

namespace TUI {
std::string add_centered(int row, int term_width, const std::string& text, int text_length) {
  /* returns string which centres text within the terminal window when printed
   */
  int col = (term_width - text_length) / 2;
  col = col < 1 ? 1 : col;
  return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H" + text;
}
std::string top_status_bar(
    const TermSize& ts, const std::string& left, const std::string& mid, const std::string& right) {
  std::string result;
  result += terminal::move_cursor(1, 1);
  result += "\033[2K\033[7m";  // invert colours
  result += std::string(ts.width, ' ');

  // split to three regions
  // " "###" "###" "###" "
  const int region_width = (ts.width - 4) / 3;
  auto truncate = [region_width](const std::string& s) {
    if (s.length() > region_width) {
      return s.substr(0, region_width - 3) + "...";
    }
    return s;
  };
  const std::string left_text = truncate(left);
  const std::string mid_text = truncate(mid);
  const std::string right_text = truncate(right);

  // add left text
  result += terminal::move_cursor(1, 1);
  result += " " + left_text;
  // add middle text
  result += add_centered(1, ts.width, mid_text, visible_length(mid_text));
  // add right text
  result += terminal::move_cursor(1, ts.width - visible_length(right_text));
  result += right_text + " ";

  result += "\x1b[0m";  // reset colours
  return result;
}

std::string bottom_status_bar(const TermSize& ts, float current_zoom_level, int rotation) {
  const std::string left_text = std::format(
      " GO TO PAGE: g {} NAVIGATE: <- -> {} QUIT : q {} "
      "Help: ? {}",
      symbols::box_single_line.at(179), symbols::box_single_line.at(179),
      symbols::box_single_line.at(179), symbols::box_single_line.at(179));
  const std::string right_text = std::format("{}{}°{} Zoom : {}%", symbols::box_single_line.at(179),
      rotation, symbols::box_single_line.at(179), current_zoom_level * 100);
  std::string result;
  result += terminal::move_cursor(ts.height, 1);  // move to last row
  result += "\033[2K\033[7m";                     // clear line and invert colours
  result += std::string(ts.width, ' ');           // overlay bg
  result += terminal::move_cursor(ts.height, 1);  // move to last row
  result += left_text;
  result += terminal::move_cursor(ts.height, ts.width - visible_length(right_text));
  result += right_text;
  result += "\033[0m";  // Reset colors
  return result;
}

std::string guard_message(const TermSize& ts) {
  terminal::hide_cursor();
  std::string result;
  result.reserve(256);  // preallocate rough estimate
  result += terminal::reset_screen_and_cursor_string();
  result += kitty::delete_image_placement();
  std::string title = "Terminal size too small";
  std::string current_dimensions = std::format("{}Width = {} {}Height = {}",
      ts.width >= MIN_COLS ? TermColor::GreenBg : TermColor::RedBg, ts.width,
      ts.height >= MIN_ROWS ? TermColor::GreenBg : TermColor::RedBg, ts.height);
  std::string required_dimensions = std::format("Needed: {} x {}", MIN_COLS, MIN_ROWS);

  // centre the text
  int center_row = ts.height / 2;
  result += add_centered(center_row - 2, ts.width, title, visible_length(title));
  result +=
      add_centered(center_row, ts.width, current_dimensions, visible_length(current_dimensions));

  result += TermColor::GreenBg;
  result += add_centered(
      center_row + 2, ts.width, required_dimensions, visible_length(required_dimensions));
  result += TermColor::Reset;
  return result;
}

std::string help_overlay(const TermSize& ts) {
  if (ts.width < MIN_COLS || ts.height < MIN_ROWS) {
    return guard_message(ts);
  }
  std::string result;
  result += terminal::save_cursor_string();

  result += TermColor::OrangeFg;
  result += TermColor::BlackBg;
  // overlay background with a black overlay
  result += terminal::move_cursor(1, 1);
  result += std::string(ts.width, ' ');  // erase top bar
  result += terminal::move_cursor(ts.height, 1);
  result += std::string(ts.width, ' ');  // erase bottom bar
  result += terminal::move_cursor(1, 1);
  result += kitty::get_dim_layer(ts.width, ts.height);
  result += terminal::move_cursor(1, 1);

  const std::string logo = R"(
        ___________________    ______  __
        ___  __ \__  __ \_ |  / /_  / / /
        __  /_/ /_  / / /_ | / /_  / / /
        _  ____/_  /_/ /__ |/ / / /_/ /
        /_/     /_____/ _____/  \____/
        )";
  std::istringstream iss(logo);
  std::string line;
  int start_row = 2;
  result += TermText::BoldText;
  while (std::getline(iss, line)) {
    if (!line.empty()) {
      line = trim(line);
      result += add_centered(start_row++, ts.width, line, 33);
    }
  }
  result += TermText::ResetBold;

  constexpr int box_height = 30;
  constexpr int box_width = 70;
  constexpr int box_start_row = 10;
  const int box_start_col = ((ts.width - box_width) / 2) + 2;
  result += create_box(
      {
          .start_row = box_start_row,
          .start_col = box_start_col,
          .width = box_width,
          .height = box_height,
      },
      true);

  // "Key" and "Description" headers in bold
  // key text in orange, middle aligned
  // description text in white, left aligned
  constexpr int key_col_width = 15;
  int text_start_row = 11;
  result += terminal::move_cursor(text_start_row, box_start_col + 1);
  result += std::format("{}{}{}", TermColor::BlackBg, TermColor::WhiteFg, TermText::BoldText);
  result += centre_with_space(key_col_width, "Key");
  result += " Description";
  // key text is centered, description text is left aligned
  result += TermText::ResetBold;
  for (auto [key_text, desc_text] : helplist::help_text) {
    text_start_row++;
    result += terminal::move_cursor(text_start_row, box_start_col + 1);
    result += std::format("{}{}", TermColor::OrangeFg, centre_with_space(key_col_width, key_text));
    result += std::format("{}{}", TermColor::WhiteFg, desc_text);
  }
  result += TermColor::Reset;
  result += terminal::restore_cursor_string();
  return result;
};

InputBarResult bottom_input_bar(
    const std::string& prompt, const InputBarDeps& deps, const std::string& error_prompt) {
  TermSize current_term_size = deps.window_dimensions();
  std::string buffer;

  int cursor_pos = 0;   // cursor index in the buffer
  int visible_pos = 0;  // index of first visible char in the buffer

  bool showing_error = !error_prompt.empty();

  auto len = [](std::string_view s) { return static_cast<int>(s.length()); };

  // redraw bar on every key action
  auto redraw = [&]() {
    std::string active_prompt;
    if (showing_error) {
      active_prompt = std::format("{}{}{}", TermColor::RedBg, error_prompt, TermColor::Reset);
    } else {
      active_prompt = prompt;
    }

    // guard message screen too small
    if (current_term_size.width < MIN_COLS || current_term_size.height < MIN_ROWS) {
      std::print("{}", guard_message(current_term_size));
      std::fflush(stdout);
      return;
    }

    // calculate correct substring start position
    // relative to current cursor and available width
    const int available_width = current_term_size.width - visible_length(active_prompt);
    visible_pos = scroll_window_start(cursor_pos, visible_pos, len(buffer), available_width);

    // clear line and draw prompt
    terminal::show_cursor();
    std::print("{}{}{}{}", terminal::move_cursor(current_term_size.height, 1),
        TermColor::InvertedBg, active_prompt, buffer.substr(visible_pos));
    std::fflush(stdout);

    // move to proper position on screen
    const int screen_col = cursor_pos - visible_pos + visible_length(active_prompt) + 1;
    std::print("{}", terminal::move_cursor(current_term_size.height, screen_col));
    std::fflush(stdout);
  };

  // reset colours, hide cursor and return buffer
  auto cleanup = [&]() {
    terminal::hide_cursor();
    std::print(TermColor::Reset);
    std::fflush(stdout);
  };

  InputEvent c;
  redraw();
  auto debouncer = ResizeDebouncer(deps.debounce_ms);
  InputBarResult result{};

  while (true) {
    const ResizeState resize_state =
        debouncer.poll(deps.was_resized(), std::chrono::steady_clock::now());
    if (resize_state == ResizeState::Resizing) {
      current_term_size = deps.window_dimensions();  // update before re-render
      redraw();
      continue;
    }

    if (resize_state == ResizeState::Settled) {
      current_term_size = deps.window_dimensions();  // update before re-render
      deps.on_resize_settled();                      // callback for expensive re-render
      redraw();
      continue;
    }

    c = deps.read_input(100);

    if (deps.window_dimensions().width < MIN_COLS || deps.window_dimensions().height < MIN_ROWS) {
      if (c.key == key_char && c.char_value == 'q') {
        result = {.value = "", .cancelled = false, .quit_requested = true};
        break;
      }
      continue;  // block other inputs if guard message is displayed
    }
    if (c.key == key_none) {  // no input, check for new frame to display
      deps.on_idle();
      redraw();
      continue;
    }
    if (c.key == key_escape) {
      result = {.value = "", .cancelled = true, .quit_requested = false};
      break;
    }
    if (c.key == key_enter) {
      result = {.value = buffer, .cancelled = false, .quit_requested = false};
      break;
    }
    if (c.key == key_backspace) {
      if (!buffer.empty() && cursor_pos > 0) {
        buffer.erase(cursor_pos - 1, cursor_pos > 0 ? 1 : 0);
        cursor_pos--;
        showing_error = false;
        redraw();
      }
    } else if (c.key == key_alt_backspace) {
      if (!buffer.empty()) {  // clear all input
        buffer = buffer.substr(cursor_pos);
        cursor_pos = 0;
        visible_pos = 0;
        showing_error = false;
        redraw();
      }
    } else if (c.key == key_char) {  // printable chars
      buffer.insert(cursor_pos, 1, c.char_value);
      cursor_pos++;
      showing_error = false;
      redraw();
    } else if (c.key == key_right_arrow) {
      if (cursor_pos < len(buffer)) {
        cursor_pos++;
        redraw();
      }
    } else if (c.key == key_left_arrow) {
      if (cursor_pos > 0) {
        cursor_pos--;
        redraw();
      }
    }
  }

  cleanup();
  return result;
}

bool is_window_too_small(const TermSize& ts) { return ts.width < MIN_COLS || ts.height < MIN_ROWS; }

float calculate_zoom_factor(
    const TermSize& ts, const pdf::PageSpecs& ps, const ContentArea& area, float zoom) {
  const int max_w_pixels = area.cols * ts.pixels_per_col;
  const int max_h_pixels = area.rows * ts.pixels_per_row;

  const float h_scale = static_cast<float>(max_w_pixels) / ps.acc_width;
  const float v_scale = static_cast<float>(max_h_pixels) / ps.acc_height;

  return std::min(h_scale, v_scale) * zoom;
}
std::string center_cursor(const TermSize& ts, int w_pixels, int h_pixels, const ContentArea& area) {
  const int cols_used =
      static_cast<int>(std::ceil(static_cast<float>(w_pixels) / ts.pixels_per_col));
  const int rows_used =
      static_cast<int>(std::ceil(static_cast<float>(h_pixels) / ts.pixels_per_row));

  int top_margin = (area.rows - rows_used) / 2;
  int left_margin = (area.cols - cols_used) / 2;

  top_margin = top_margin > 0 ? top_margin : 0;
  left_margin = left_margin > 0 ? left_margin : 0;
  return terminal::move_cursor(top_margin + area.start_row, left_margin + area.start_col);
}
}  // namespace TUI