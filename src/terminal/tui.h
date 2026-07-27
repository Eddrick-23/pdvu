#pragma once
#include <functional>
#include <string>

#include "render/parser.h"
#include "terminal.h"

namespace TUI::symbols {

inline const std::unordered_map<int, std::string_view> box_single_line = {
    {191, "\u2510"},  // ┐
    {192, "\u2514"},  // └
    {193, "\u2534"},  // ┴
    {194, "\u252C"},  // ┬
    {195, "\u251C"},  // ├
    {196, "\u2500"},  // ─
    {197, "\u253C"},  // ┼
    {217, "\u2518"},  // ┘
    {218, "\u250C"},  // ┌
    {179, "\u2502"},  // │
    {180, "\u2524"}   // ┤
};

inline const std::unordered_map<int, std::string_view> box_double_line = {
    {185, "\u2563"},  // ╣
    {186, "\u2551"},  // ║
    {187, "\u2557"},  // ╗
    {188, "\u255D"},  // ╝
    {200, "\u255A"},  // ╚
    {201, "\u2554"},  // ╔
    {202, "\u2569"},  // ╩
    {203, "\u2566"},  // ╦
    {204, "\u2560"},  // ╠
    {205, "\u2550"},  // ═
    {206, "\u256C"}   // ╬
};

}  // namespace TUI::symbols
namespace TUI {
// min dimensions for displaying guard message
inline constexpr int MIN_ROWS = 40;
inline constexpr int MIN_COLS = 40;
std::string add_centered(int row, int term_width, const std::string& text, int text_length);
std::string top_status_bar(
    const TermSize& ts, const std::string& left, const std::string& mid, const std::string& right);
std::string bottom_status_bar(const TermSize& ts, float current_zoom_level, int rotation);
std::string guard_message(const TermSize& ts);
std::string help_overlay(const TermSize& ts);

/**
 * @brief Dependencies bottom_input_bar needs from the caller's terminal and
 * render loop.
 *
 * on_idle and on_resize_settled are both "nothing to type, do your own
 * housekeeping" hooks, but fire for different reasons - keep them separate
 * rather than reusing one for both. A settled resize is usually the one
 * moment worth doing expensive work (e.g. re-requesting a fresh render at current
 * resolution), while idle ticks are for cheap, frequent checks (e.g. picking
 * up a frame that finished rendering in the background).
 */
struct InputBarDeps {
  std::function<TermSize()> window_dimensions;           ///< Current Terminal Size.
  std::function<bool()> was_resized;                     ///< Raw per-tick resize signal.
  std::function<InputEvent(int timeout_ms)> read_input;  ///< Blocking read with timeout.
  std::function<void()> on_idle;            ///< Called on ticks with no input and no resize
  std::function<void()> on_resize_settled;  ///< Called once, when a resize just settled.
  int debounce_ms;                          ///< debounce interval in milliseconds
};

/**
 * @brief Outcome of a bottom_input_bar() call.
 */
struct InputBarResult {
  std::string value;            ///< stored value in input buffer
  bool cancelled = false;       ///< Esc pressed to exit ui
  bool quit_requested = false;  ///< 'q' or 'esc' pressed while quard message showing
};

/**
 * @brief Runs a single-row text input UI on the bottom line of the terminal
 * until the user presses Enter or Escape, or requests to quit.
 *
 * Draws `prompt` followed by an editable text buffer, supporting cursor
 * movement, backspace (character-at-a-time and alt-backspace to clear
 * everything before the cursor), and horizontal scrolling when the buffer
 * exceeds the available width.
 *
 * While running, this function also drives its own resize-debounce loop:
 * every tick a resize is in progress it refreshes the cached terminal size
 * and redraws; once the resize settles it invokes `deps.on_resize_settled()`
 * (exactly once per resize) before redrawing again. On ticks with no input
 * at all, it invokes `deps.on_idle()` instead - callers typically use this
 * to pick up newly-rendered frames without blocking the input loop.
 *
 * If the terminal is smaller than TUI::MIN_COLS x TUI::MIN_ROWS, typing is
 * blocked and a guard message is shown in place of the prompt; the only
 * accepted key in that state is 'q' or 'esc', which exits immediately with
 * `quit_requested = true` so the caller can propagate the quit up rather
 * than treating it as a normal cancel.
 *
 * @param prompt Text shown before the editable buffer, e.g. "Go to page: ".
 * @param deps Callbacks and terminal queries this function needs - see
 *             InputBarDeps. None of the std::function members may be empty.
 * @return An InputBarResult - see its field docs for how to distinguish
 *         a submitted value from a cancel or a quit request.
 */
InputBarResult bottom_input_bar(const std::string& prompt, const InputBarDeps& deps);

bool is_window_too_small(const TermSize& ts);
float calculate_zoom_factor(
    const TermSize& ts, const pdf::PageSpecs&, int content_cols, int content_rows, float zoom);
std::string center_cursor(const TermSize& ts, int w_pixels, int h_pixels, int content_cols,
    int content_rows, int start_row, int start_col);
}  // namespace TUI
