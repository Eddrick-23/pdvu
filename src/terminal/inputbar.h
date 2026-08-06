#pragma once
#include <string>

#include "terminal.h"
#include "viewer/keys.h"

namespace TUI {
/**
 * @brief Stateful single-line text input component for a terminal bottom bar.
 *
 * InputBar owns the editable buffer, insertion cursor, horizontal scrolling
 * position, and optional error prompt. It processes one InputEvent at a time
 * and produces terminal escape sequences for the current state, but does not
 * write to the terminal itself or control the application's UI mode.
 *
 * Submission and cancellation do not reset the component. The caller decides
 * how to validate the value, change modes, and reset the component.
 *
 * @note Editing is currently byte-oriented and expects ASCII input through
 * InputEvent::char_value.
 */
class InputBar {
 public:
  /**
   * @brief Describes the result of processing one input event.
   *
   * Caller can use this result to decide whether to redraw the input bar,
   * submit its value, leave the current UI mode, or take no action.
   */
  enum class Action {
    None,
    Changed,
    Submitted,
    Cancelled,
  };

  /**
   * @brief Constructs an empty input bar with the supplied prompt.
   *
   * @param prompt Text displayed before the editable buffer.
   */
  explicit InputBar(std::string_view prompt);

  /**
   * @brief Applies one input event to the input bar state.
   *
   * Supported events include character insertion, backspace, cursor movement,
   * submission, and cancellation. Unsupported events are ignored.
   *
   * @param event Decoded terminal input event.
   * @return Action describing the result of processing the event.
   */
  Action handle(const InputEvent& event);

  /**
   * @brief Builds the terminal sequence for drawing the input bar.
   *
   * The input bar is drawn on the terminal's bottom row. When the buffer is
   * wider than the available space, the visible window is adjusted to keep
   * the insertion cursor on screen.
   *
   * @param ts Current terminal dimensions.
   * @return ANSI sequence that draws the input bar and positions its cursor,
   *         or an empty string when there is insufficient horizontal space.
   */
  std::string render_sequence(const TermSize& ts);

  /**
   * @brief Returns the complete input buffer, including text currently outside
   * the visible scrolling window.
   *
   * @return Non-owning view of the current buffer.
   *
   * @note The returned view may be invalidated by a subsequent call to
   * handle() or reset().
   */
  [[nodiscard]] std::string_view value() const;

  /**
   * @brief Replaces the normal prompt with an error prompt.
   *
   * The editable buffer and cursor position are preserved.
   *
   * @param error Error prompt to display.
   */
  void set_error(std::string error);

  /**
   * @brief Removes the active error prompt and restores the normal prompt.
   */
  void clear_error();

  /**
   * @brief Restores the component to its initial empty state.
   *
   * Clears the input buffer and error prompt, then resets the insertion cursor
   * and horizontal scrolling position.
   */
  void reset();

 private:
  std::string m_prompt;
  std::string m_buffer;
  std::string m_error_prompt;

  std::size_t m_cursor_pos = 0;
  std::size_t m_visible_pos = 0;
};
}  // namespace TUI