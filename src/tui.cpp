#include <string>
#include "terminal.h"
#include "tui.h"

namespace { // utility function
    int visible_length(const std::string &s) {
        int count = 0;
        bool in_escape = false;
        for (size_t i = 0; i < s.length(); ++i) {
            if (!in_escape) {
                if (s[i] == '\033') {
                    in_escape = true;
                } else {
                    count++;
                }
            } else {
                if (s[i] == 'm') {
                    in_escape = false;
                }
            }
        }
        return count;
    }
}

namespace TUI {
    std::string top_bar(const TermSize& ts, const std::string& left, const std::string& mid, const std::string& right) {
        std::string result;
        result += terminal::move_cursor(1, 1);
        result +=  "\033[2K\033[7m"; // invert colours

        // each char takes a col, calculate indent
        // split to three regions
        // " "###" "###" "###" "
        int region_width = (ts.width - 4) / 3;
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
        result += " " + left_text + std::string(region_width - left_text.length(), ' ');

        // add middle text
        int mid_pad_left = (region_width - mid_text.length()) / 2;
        int mid_pad_right = region_width - mid_text.length() - mid_pad_left;
        result += " " + std::string(mid_pad_left, ' ') + mid_text + std::string(mid_pad_right, ' ');

        // add right text
        result += " " + std::string(region_width - right_text.length(), ' ') + right_text + " ";

        result += "\033[0m"; // reset colours
        return result;
    }

    std::string bottom_bar(const TermSize& ts) {
        const std::string text = " SEARCH: ctrl/cmd + f | NAVIGATE: <- -> | QUIT : q | Help: ?";
        int padding = ts.width - text.length();
        if (padding < 0) padding = 0;

        std::string result;
        result += terminal::move_cursor(ts.height, 1);  // move to last row
        result += "\033[2K\033[7m"; // clear line and invert colours
        result += text; // text for bottom bar
        result += std::string(padding, ' ');
        result += "\033[0m"; // Reset colors
        return result;
    }

    std::string guard_message(const TermSize& ts) {
        std::string result;
        const std::string red ="\033[1;31m";
        const std::string green ="\033[1;32m";
        std::string title = "Terminal size too small";
        std::string current_dimensions = (ts.width >= MIN_COLS ? green : red) + "Width = " +  std::to_string(ts.width)
                                         + (ts.height >= MIN_ROWS ? green : red) + " Height = " +  std::to_string(ts.height);
        std::string required_dimensions = "Needed: " + std::to_string(MIN_COLS) + " x " + std::to_string(MIN_ROWS);

        // centre the text
        auto add_centered = [&](int r, const std::string& text, const int text_len) {
            int c = (ts.width - text_len) / 2;
            if (c < 1) c = 1;
            result += "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H" + text;
        };

        int center_row = ts.height / 2;

        add_centered(center_row - 2, title, title.length());
        add_centered(center_row, current_dimensions, visible_length(current_dimensions));

        result += green;
        add_centered(center_row + 2, required_dimensions, required_dimensions.length());
        result += "\033[0m";       // Reset
        return result;
    }
    // TODO
    // std::string help_message(const TermSize& ts) {}
}

