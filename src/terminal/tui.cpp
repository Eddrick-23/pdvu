#include "tui.h"

#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "ansi.h"
#include "kitty.h"
#include "terminal.h"
#include "tui_internal.h"
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

namespace TUI {
std::string add_centered(int row, int term_width, const std::string& text, int text_length) {
  /* returns string which centres text within the terminal window when printed
   */
  int col = (term_width - text_length) / 2;
  col = col < 1 ? 1 : col;
  return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H" + text;
}
std::string top_status_bar(const TermSize& ts, const std::string& left, const std::string& mid,
                           const std::string& right) {
  std::string result;
  result += terminal::move_cursor(1, 1);
  result += "\033[2K\033[7m";  // invert colours
  result += std::string(static_cast<std::size_t>(ts.columns), ' ');

  // split to three regions
  // " "###" "###" "###" "
  const auto region_width = static_cast<std::size_t>((ts.columns - 4) / 3);
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
  result += add_centered(1, ts.columns, mid_text, helpers::visible_length(mid_text));
  // add right text
  result += terminal::move_cursor(1, ts.columns - helpers::visible_length(right_text));
  result += right_text + " ";

  result += "\x1b[0m";  // reset colours
  return result;
}

std::string bottom_status_bar(const TermSize& ts, float current_zoom_level, int rotation) {
  const std::string left_text = std::format(
      " GO TO PAGE: g {} NAVIGATE: <- -> {} QUIT : q {} "
      "Help: ? {}",
      symbols::box_single_line.at(179),
      symbols::box_single_line.at(179),
      symbols::box_single_line.at(179),
      symbols::box_single_line.at(179));
  const std::string right_text = std::format("{}{}°{} Zoom : {}%",
                                             symbols::box_single_line.at(179),
                                             rotation,
                                             symbols::box_single_line.at(179),
                                             current_zoom_level * 100);
  std::string result;
  result += terminal::move_cursor(ts.rows, 1);  // move to last row
  result += "\033[2K\033[7m";                   // clear line and invert colours
  result += std::string(static_cast<std::size_t>(ts.columns), ' ');  // overlay bg
  result += terminal::move_cursor(ts.rows, 1);                       // move to last row
  result += left_text;
  result += terminal::move_cursor(ts.rows, ts.columns - helpers::visible_length(right_text));
  result += right_text;
  result += "\033[0m";  // Reset colors
  return result;
}

std::string guard_message(const TermSize& ts) {
  terminal::hide_cursor();
  std::string result;
  result.reserve(256);  // preallocate rough estimate
  result += TermColor::Reset;
  result += terminal::reset_screen_and_cursor_string();
  result += kitty::delete_image_placement();
  std::string title = "Terminal size too small";
  std::string current_dimensions =
      std::format("{}Width = {} {}Height = {}",
                  ts.columns >= MIN_COLS ? TermColor::GreenBg : TermColor::RedBg,
                  ts.columns,
                  ts.rows >= MIN_ROWS ? TermColor::GreenBg : TermColor::RedBg,
                  ts.rows);
  std::string required_dimensions = std::format("Needed: {} x {}", MIN_COLS, MIN_ROWS);

  // centre the text
  int center_row = ts.rows / 2;
  result += add_centered(center_row - 2, ts.columns, title, helpers::visible_length(title));
  result += add_centered(
      center_row, ts.columns, current_dimensions, helpers::visible_length(current_dimensions));

  result += TermColor::GreenBg;
  result += add_centered(center_row + 2,
                         ts.columns,
                         required_dimensions,
                         helpers::visible_length(required_dimensions));
  result += TermColor::Reset;
  return result;
}

std::string help_overlay(const TermSize& ts) {
  if (ts.columns < MIN_COLS || ts.rows < MIN_ROWS) {
    return guard_message(ts);
  }
  std::string result;
  result += terminal::save_cursor_string();

  result += TermColor::OrangeFg;
  result += TermColor::BlackBg;
  // overlay background with a black overlay
  result += terminal::move_cursor(1, 1);
  result += std::string(static_cast<std::size_t>(ts.columns), ' ');  // erase top bar
  result += terminal::move_cursor(ts.rows, 1);
  result += std::string(static_cast<std::size_t>(ts.columns), ' ');  // erase bottom bar
  result += terminal::move_cursor(1, 1);
  result += kitty::get_dim_layer(ts.columns, ts.rows);
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
      line = helpers::trim(line);
      result += add_centered(start_row++, ts.columns, line, 33);
    }
  }
  result += TermText::ResetBold;

  constexpr int box_height = 30;
  constexpr int box_width = 70;
  constexpr int box_start_row = 10;
  const int box_start_col = ((ts.columns - box_width) / 2) + 2;
  result += helpers::create_box(
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
  result += helpers::centre_with_space(key_col_width, "Key");
  result += " Description";
  // key text is centered, description text is left aligned
  result += TermText::ResetBold;
  for (auto [key_text, desc_text] : helplist::help_text) {
    text_start_row++;
    result += terminal::move_cursor(text_start_row, box_start_col + 1);
    result += std::format(
        "{}{}", TermColor::OrangeFg, helpers::centre_with_space(key_col_width, key_text));
    result += std::format("{}{}", TermColor::WhiteFg, desc_text);
  }
  result += TermColor::Reset;
  result += terminal::restore_cursor_string();
  return result;
};

bool is_window_too_small(const TermSize& ts) { return ts.columns < MIN_COLS || ts.rows < MIN_ROWS; }

float calculate_zoom_factor(const TermSize& ts, const pdf::PageSpecs& ps, const ContentArea& area,
                            float zoom) {
  const int max_w_pixels = area.cols * ts.cell_pixel_width;
  const int max_h_pixels = area.rows * ts.cell_pixel_height;

  const float h_scale = static_cast<float>(max_w_pixels) / ps.acc_width;
  const float v_scale = static_cast<float>(max_h_pixels) / ps.acc_height;

  return std::min(h_scale, v_scale) * zoom;
}

geometry::CellPosition centered_cursor_position(const TermSize& ts, int w_pixels, int h_pixels,
                                                const ContentArea& area) {
  const auto ceil_div = [](int value, int divisor) {
    assert(value >= 0);
    assert(divisor > 0);

    return (value / divisor) + (value % divisor != 0);
  };

  const int cols_used = ceil_div(w_pixels, ts.cell_pixel_width);
  const int rows_used = ceil_div(h_pixels, ts.cell_pixel_height);

  int top_margin = (area.rows - rows_used) / 2;
  int left_margin = (area.cols - cols_used) / 2;

  top_margin = top_margin > 0 ? top_margin : 0;
  left_margin = left_margin > 0 ? left_margin : 0;

  return geometry::CellPosition{
      .row = top_margin + area.start_row,
      .col = left_margin + area.start_col,
  };
}
}  // namespace TUI