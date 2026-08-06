#pragma once
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include "terminal.h"
#include "tui.h"
namespace TUI::helpers {  // utility functions
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
inline int visible_length(const std::string& s) {
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
inline std::string centre_with_space(int width, std::string_view text) {
  const int text_len = static_cast<int>(std::ssize(text));
  const int left_padding = std::max(0, (width - text_len) / 2);
  const int right_padding = std::max(0, width - text_len - left_padding);
  return std::string(static_cast<std::size_t>(left_padding), ' ') + std::string(text) +
         std::string(static_cast<std::size_t>(right_padding), ' ');
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
inline std::string trim(std::string_view str, const std::string& whitespace = " \t") {
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
inline std::string create_box(const BoxBounds& box_bounds, bool fill) {
  auto [start_row, start_col, width, height] = box_bounds;
  std::string result = fill ? std::format("\x1b[48;5;{}m", 16) : "";
  result += terminal::move_cursor(start_row, start_col);
  result += symbols::box_double_line.at(201);
  for (int i = 0; i < width - 2; i++) {
    result += symbols::box_single_line.at(196);
  }
  result += symbols::box_double_line.at(187);

  for (int i = start_row + 1; i < start_row + height; i++) {  // draw the sides
    result += terminal::move_cursor(i, start_col);
    result += symbols::box_single_line.at(179);
    if (fill) {
      result += std::string(static_cast<std::size_t>(width - 2), ' ');
    } else {
      result += terminal::move_cursor(i, start_col + width - 1);
    }
    result += symbols::box_single_line.at(179);
  }
  result += terminal::move_cursor(start_row + height, start_col);
  result += symbols::box_double_line.at(200);
  for (int i = 0; i < width - 2; i++) {
    result += symbols::box_single_line.at(196);
  }
  result += symbols::box_double_line.at(188);
  result += fill ? "\x1b[49m" : "";  // reset bg colour
  return result;
}
}  // namespace TUI::helpers