#include "tui.h"
#include "kitty.h"
#include "terminal.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <print>
#include <string>

namespace TUI::helplist {
const std::vector<std::array<std::string, 2>> help_text = {
    {"->", "Next Page"},
    {"<-", "Previous Page"},
    {"q", "Quit"},
    {"g", "Go to Page"},
    {"Esc", "Exit input textbox"},
    {"/ or shift+f", "Find text"},
    {"w", "Pan up"},
    {"a", "Pan left"},
    {"s", "Pan down"},
    {"d", "pan right"},
    {"r", "rotate clockwise 90 degrees"},
    {"+ or =", "zoom in"},
    {"- or _", "zoom out"},
    {"f", "zoom to fit"},
    {"?", "Help page"},
};
}

namespace { // utility function
int visible_length(const std::string &s) {
  /* count visible length of string ignoring escape characters*/
  int count = 0;
  bool in_escape = false;
  for (size_t i = 0; i < s.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == '\x1b') { // start of escape character
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

    if ((c & 0xC0) == 0x80) {
      continue;
    }
    count++; // stand char or start of a utf-8 char
  }
  return count;
}

std::string centre_with_space(int width, const std::string &text) {
  /* return a string centred in a given width, with spaced padding on its sides
   */
  std::string result;
  const int left_padding = (width - text.length()) / 2;
  const int right_padding = width - text.length() - left_padding;
  result +=
      std::string(left_padding, ' ') + text + std::string(right_padding, ' ');
  return result;
}
std::string trim(const std::string &str,
                 const std::string &whitespace = " \t") {
  const auto strBegin = str.find_first_not_of(whitespace);
  if (strBegin == std::string::npos)
    return ""; // no content

  const auto strEnd = str.find_last_not_of(whitespace);
  const auto strRange = strEnd - strBegin + 1;

  return str.substr(strBegin, strRange);
}
std::string create_box(int start_row, int start_col, int width, int height,
                       bool fill) {

  std::string result = fill ? std::format("\x1b[48;5;{}m", 16) : "";
  result += terminal::move_cursor(start_row, start_col);
  result += TUI::symbols::box_double_line.at(201);
  for (int i = 0; i < width - 2; i++) {
    result += TUI::symbols::box_single_line.at(196);
  }
  result += TUI::symbols::box_double_line.at(187);

  for (int i = start_row + 1; i < start_row + height; i++) { // draw the sides
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
  result += fill ? "\x1b[49m" : ""; // reset bg colour
  return result;
}
} // namespace

namespace TUI {
std::string add_centered(int row, int term_width, const std::string &text,
                         int text_length) {
  /* returns string which centres text within the terminal window when printed
   */
  int col = (term_width - text_length) / 2;
  col = col < 1 ? 1 : col;
  return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H" + text;
}
std::string top_bar(const TermSize &ts, const std::string &left,
                    const std::string &mid, const std::string &right) {
  std::string result;
  result += terminal::move_cursor(1, 1);
  result += "\033[2K\033[7m"; // invert colours
  result += std::string(ts.width, ' ');

  // split to three regions
  // " "###" "###" "###" "
  const int region_width = (ts.width - 4) / 3;
  auto truncate = [region_width](const std::string &s) {
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
  result += add_centered(1, ts.width, mid_text, visible_length((mid_text)));
  // add right text
  result += terminal::move_cursor(1, ts.width - visible_length(right_text));
  result += right_text + " ";

  result += "\x1b[0m"; // reset colours
  return result;
}

std::string bottom_bar(const TermSize &ts) {
  const std::string text = std::format(
      "SEARCH: ctrl/cmd + f {} GO TO PAGE: g {} NAVIGATE: <- -> {} QUIT : q {} "
      "Help: ?",
      symbols::box_single_line.at(179), symbols::box_single_line.at(179),
      symbols::box_single_line.at(179), symbols::box_single_line.at(179));

  std::string result;
  result += terminal::move_cursor(ts.height, 1); // move to last row
  result += "\033[2K\033[7m";           // clear line and invert colours
  result += std::string(ts.width, ' '); // overlay bg
  result += terminal::move_cursor(ts.height, 1); // move to last row
  result += add_centered(ts.height, ts.width, text,
                         visible_length(text)); // text for bottom bar
  result += "\033[0m";                          // Reset colors
  return result;
}

std::string guard_message(const TermSize &ts) {
  terminal::hide_cursor();
  std::string result;
  result += terminal::reset_screen_and_cursor_string();
  const std::string red = "\x1b[1;31m";
  const std::string green = "\x1b[1;32m";
  std::string title = "Terminal size too small";
  std::string current_dimensions = (ts.width >= MIN_COLS ? green : red) +
                                   "Width = " + std::to_string(ts.width) +
                                   (ts.height >= MIN_ROWS ? green : red) +
                                   " Height = " + std::to_string(ts.height);
  std::string required_dimensions =
      "Needed: " + std::to_string(MIN_COLS) + " x " + std::to_string(MIN_ROWS);

  // centre the text

  int center_row = ts.height / 2;

  result +=
      add_centered(center_row - 2, ts.width, title, visible_length(title));
  result += add_centered(center_row, ts.width, current_dimensions,
                         visible_length(current_dimensions));

  result += green;
  result += add_centered(center_row + 2, ts.width, required_dimensions,
                         visible_length(required_dimensions));
  result += "\x1b[0m"; // Reset
  return result;
}
// TODO
std::string help_overlay(const TermSize &ts) {
  if (ts.width < MIN_COLS || ts.height < MIN_ROWS) {
    return guard_message(ts);
  }
  std::string result;
  result += std::format("\x1b{}", 7);
  const std::string orange_fg = std::format("\x1b[38;5;{}m", 214);
  const std::string white_fg = std::format("\x1b[38;5;{}m", 255);
  const std::string bold_text = "\x1b[1m";
  const std::string reset_bold = "\x1b[22m";
  const std::string black_bg = std::format("\x1b[48;5;{}m", 16);
  result += orange_fg; // fg
  // overlay background with a black overlay
  result += terminal::move_cursor(1, 1);
  result += get_dim_layer(ts.width, ts.height);

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
  result += "\x1b[1m"; // set bold text
  while (std::getline(iss, line)) {
    if (!line.empty()) {
      line = trim(line);
      result += add_centered(start_row++, ts.width, line, 33);
    }
  }
  result += "\x1b[22m"; // reset bold

  constexpr int box_height = 30;
  constexpr int box_width = 70;
  constexpr int box_start_row = 10;
  const int box_start_col = (ts.width - box_width) / 2 + 2;
  result +=
      create_box(box_start_row, box_start_col, box_width, box_height, true);

  // "Key" and "Description" headers in bold
  // key text in orange, middle aligned
  // description text in white, left aligned
  constexpr int key_col_width = 15;
  int text_start_row = 11;
  result += terminal::move_cursor(text_start_row, box_start_col + 1);
  result += black_bg + white_fg + bold_text;
  result += centre_with_space(key_col_width, "Key");
  result += " Description";
  // key text is centered, description text is left aligned
  result += reset_bold;
  for (auto arr : helplist::help_text) {
    text_start_row++;
    result += terminal::move_cursor(text_start_row, box_start_col + 1);
    const std::string &key_text = arr.front();
    const std::string &desc_text = arr.back();
    result += orange_fg + centre_with_space(key_col_width, key_text);
    result += white_fg + " " + desc_text;
  }
  result += "\x1b[0m";                // reset colors
  result += std::format("\x1b{}", 8); // restore
  return result;
};

std::string bottom_input_bar(Terminal &term, const std::string &prompt,
                             const std::function<void()> &on_resize) {
  /* creates a single row text input UI that takes user input */
  // Might be a better way to handle resizing but for now we pass in a callback
  // function

  TermSize current_term_size = term.get_terminal_size();
  std::string buffer;

  size_t cursor_pos = 0;  // cursor index in the buffer
  size_t visible_pos = 0; // index of first visible char in buffer

  auto redraw = [&]() { // helper to redraw the whole line on input
    if (current_term_size.width < MIN_COLS ||
        current_term_size.height < MIN_ROWS) {
      std::print("{}", guard_message(current_term_size));
      std::fflush(stdout);
      return;
    }
    int available_width = current_term_size.width - prompt.length();
    available_width = available_width < 1 ? 1 : available_width;

    if (cursor_pos < visible_pos) { // scrolled left off screen
      visible_pos = cursor_pos;
    } else if (cursor_pos >=
               visible_pos + available_width) { // scrolled right off screen
      visible_pos = cursor_pos - available_width + 1;
    }

    // If buffer is longer than screen, always show the rightmost portion
    if (buffer.length() > available_width) {
      visible_pos = buffer.length() - available_width;
    }
    // clear line and draw prompt
    terminal::show_cursor();
    std::print("{}{}{}{}", terminal::move_cursor(current_term_size.height, 1),
               "\x1b[2K\x1b[7m", prompt, buffer.substr(visible_pos));
    std::fflush(stdout);

    // move to proper position on screen
    size_t screen_col = cursor_pos - visible_pos + prompt.length() + 1;
    std::print("{}",
               terminal::move_cursor(current_term_size.height, screen_col));
    std::fflush(stdout);
  };
  InputEvent c;
  redraw();
  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  bool resizing = false;
  while (true) {
    if (term.was_resized()) {
      resizing = true;
      start = Clock::now();
      continue;
    }
    if (resizing) {
      auto now = Clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
      if (duration.count() > 75) {                    // exit and rerender
        current_term_size = term.get_terminal_size(); // update before rerender
        resizing = false;
        on_resize(); // under new implementation, callback must automatically
                     // render as well
        redraw();
        continue;
      }
    }
    c = term.read_input(100);

    if (term.get_terminal_size().width < MIN_COLS ||
        term.get_terminal_size().height < MIN_ROWS) {
      // TODO find another way to allow direct quit from here?
      if (c.key == key_char && c.char_value == 'q') {
        return "";
      }
      continue; // block inputs if guard message is displayed
    }
    if (c.key == key_none) { // no input, check for new frame to display
      on_resize();
      // redraw();
      continue;
    }
    // break when we get esc and clear buffer
    if (c.key == key_escape) {
      buffer.clear();
      break;
    }
    // break when we get enter
    if (c.key == key_enter) {
      break;
    }
    if (c.key == key_backspace) {
      if (!buffer.empty()) {
        buffer.erase(cursor_pos - 1, cursor_pos > 0 ? 1 : 0);
        cursor_pos--;
        redraw();
      }
    } else if (c.key == key_alt_backspace) {
      if (!buffer.empty()) { // clear all input
        buffer = buffer.substr(cursor_pos);
        cursor_pos = 0;
        visible_pos = 0;
        redraw();
      }
    } else if (c.key == key_char) { // printable chars
      buffer.insert(cursor_pos, 1, c.char_value);
      cursor_pos++;
      redraw();
    } else if (c.key == key_right_arrow) {
      if (cursor_pos < buffer.length()) {
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

  // reset colours, hide cursor and return buffer
  std::print("{}", "\x1b[0m");
  terminal::hide_cursor();
  return buffer;
}

}