#include "inputbar.h"

#include <format>

#include "ansi.h"
#include "tui_internal.h"

namespace {
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
std::size_t scroll_window_start(std::size_t cursor_pos, std::size_t visible_pos,
                                std::size_t buffer_len, std::size_t available_width) {
  cursor_pos = std::min(cursor_pos, buffer_len);
  visible_pos = std::min(visible_pos, buffer_len);
  if (cursor_pos < visible_pos) {  // cursor crossed left edge
    visible_pos = cursor_pos;
  } else if (cursor_pos >= visible_pos + available_width) {  // cursor crossed right edge
    visible_pos = cursor_pos - available_width + 1;
  }
  // ensure resizing or deletion cannot leave empty space to the left
  // when more of the buffer could be shown
  // +1 reserves a cell for the insertion cursor after the final char
  const std::size_t maximum_visible_pos =
      buffer_len >= available_width ? buffer_len - available_width + 1 : 0;
  return std::min(visible_pos, maximum_visible_pos);
}
}  // namespace

namespace TUI {
InputBar::InputBar(std::string_view prompt) : m_prompt(prompt) {}

InputBar::Action InputBar::handle(const InputEvent& event) {
  switch (event.key) {
    case key_enter:
      return Action::Submitted;
    case key_escape:
      return Action::Cancelled;
    case key_backspace:
      if (m_cursor_pos > 0) {
        m_buffer.erase(m_cursor_pos - 1, 1);
        --m_cursor_pos;
        return Action::Changed;
      }
      return Action::None;
    case key_alt_backspace:
      if (m_cursor_pos > 0) {
        m_buffer.erase(0, m_cursor_pos);
        m_cursor_pos = 0;
        return Action::Changed;
      }
      return Action::None;
    case key_left_arrow:
      if (m_cursor_pos > 0) {
        --m_cursor_pos;
        return Action::Changed;
      }
      return Action::None;
    case key_right_arrow:
      if (m_cursor_pos < m_buffer.size()) {
        ++m_cursor_pos;
        return Action::Changed;
      }
      return Action::None;
    case key_char:
      m_buffer.insert(m_cursor_pos, 1, event.char_value);
      ++m_cursor_pos;
      return Action::Changed;
    default:
      return Action::None;
  }
}

std::string InputBar::render_sequence(const TermSize& ts) {
  const bool showing_error = !m_error_prompt.empty();
  std::string active_prompt;
  if (showing_error) {
    active_prompt = std::format("{}{}{}", TermColor::RedBg, m_error_prompt, TermColor::Reset);
  } else {
    active_prompt = m_prompt;
  }
  const int available_columns = ts.columns - helpers::visible_length(active_prompt);
  if (available_columns <= 0) {
    return "";
  }

  const auto available_width = static_cast<std::size_t>(available_columns);
  m_visible_pos =
      scroll_window_start(m_cursor_pos, m_visible_pos, m_buffer.size(), available_width);

  std::string clear_line_and_draw_prompt =
      std::format("{}{}{}{}",
                  terminal::move_cursor(ts.rows, 1),
                  TermColor::InvertedBg,
                  active_prompt,
                  m_buffer.substr(m_visible_pos, available_width));
  // sequence to move cursor to correct position
  const int screen_col = static_cast<int>(m_cursor_pos) - static_cast<int>(m_visible_pos) +
                         helpers::visible_length(active_prompt) + 1;
  std::string move_cursor_to_correct_position = terminal::move_cursor(ts.rows, screen_col);
  return std::format("{}{}", clear_line_and_draw_prompt, move_cursor_to_correct_position);
}

std::string_view InputBar::value() const { return m_buffer; }

void InputBar::set_error(std::string error) { m_error_prompt = error; }
void InputBar::clear_error() { m_error_prompt.clear(); }
void InputBar::reset() {
  m_buffer.clear();
  m_error_prompt.clear();
  m_cursor_pos = 0;
  m_visible_pos = 0;
}
}  // namespace TUI