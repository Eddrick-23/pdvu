#pragma once
#include <functional>
#include <string>

#include "render/parser.h"
#include "terminal.h"

namespace TUI::symbols {

/// Single-line box-drawing characters, keyed by their legacy DOS/CP437 code
/// point (matches the numbering used in classic box-drawing character
/// charts, not Unicode code points).
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
    {180, "\u2524"},  // ┤
};

/// Double-line box-drawing characters, keyed the same way as box_single_line.
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
    {206, "\u256C"},  // ╬
};

}  // namespace TUI::symbols
namespace TUI {
// min dimensions for displaying guard message
inline constexpr int MIN_ROWS = 40;
inline constexpr int MIN_COLS = 40;

/**
 * @brief Returns the escape sequence that moves the cursor to `row` and prints
 * `text` starting at the column that centres it within `term_width`.
 *
 * @param row target row
 * @param term_width terminal width in terms of cells
 * @param text text to display
 * @param text_length caller-supplied display width of text. (use visible_length() for
 *                    strings containing ANSI escapes or multi-byte UTF-8)
 */
std::string add_centered(int row, int term_width, const std::string& text, int text_length);

/// Returns the sequence to produce an inverted-colour top bar: `left` flush left,
/// `mid` centred, `right` flush right, each truncated with "..." if it would overlap
/// the others.
std::string top_status_bar(
    const TermSize& ts, const std::string& left, const std::string& mid, const std::string& right);

/// Returns the sequence to produce inverted-colour bottom bar showing the
/// key-binding hints, current rotation, and current zoom level.
std::string bottom_status_bar(const TermSize& ts, float current_zoom_level, int rotation);

/// Renders the "terminal too small" message shown in place of normal UI
/// whenever the terminal is below MIN_COLS x MIN_ROWS.
std::string guard_message(const TermSize& ts);

/// Renders the full-screen help overlay (key-binding reference). Falls back
/// to guard_message() if the terminal is currently too small to fit it.
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
 * @param error_prompt Optional error prompt to pass in if user gives invalid input. Empty strings
 *                     are taken as no error_prompt
 * @return An InputBarResult - see its field docs for how to distinguish
 *         a submitted value from a cancel or a quit request.
 */
InputBarResult bottom_input_bar(
    const std::string& prompt, const InputBarDeps& deps, const std::string& error_prompt);

/// True if the terminal is smaller than MIN_COLS x MIN_ROWS, i.e. too small
/// to draw normal UI.
bool is_window_too_small(const TermSize& ts);

/**
 * @brief A rectangular drawable region within the terminal, in cells - used
 * to describe the space available for page content below/above the top and
 * bottom status bars.
 */
struct ContentArea {
  int cols;           ///< Width of the area, in terminal cells.
  int rows;           ///< Height of the area, in terminal cells.
  int start_row = 1;  ///< Row of the area's top-left cell.
  int start_col = 1;  ///< Column of the area's top-left cell.
};

/**
 * @brief Computes the zoom factor that scales a page to fit within the
 * available content area, then applies an additional user zoom on top.
 *
 * The fit-to-content scale is computed independently for width and height
 * (converting the cell-based content area into pixels via
 * ts.pixels_per_col/pixels_per_row), then the smaller of the two is used -
 * so the page is scaled down to fit whichever dimension is more
 * constraining, preserving aspect ratio rather than stretching to fill both.
 *
 * @param ts Current terminal size, used for its pixels_per_col/
 *           pixels_per_row to convert content_cols/content_rows from cells
 *           into pixels.
 * @param ps Target page's specs; only acc_width and acc_height are used -
 *           the page's current accumulated pixel dimensions being fit
 *           against the available area.
 * @param area Available drawing area; only cols and rows are used here
 *             (start_row/start_col don't affect the scale, only where the
 *             result is later drawn - see center_cursor()).
 * @param zoom Additional user zoom multiplier applied on top of the
 *             fit-to-content scale (1.0 = fit exactly, >1.0 = zoomed in
 *             beyond fit, <1.0 = zoomed out).
 * @return Combined scale factor to multiply the page's pixel dimensions by.
 */
float calculate_zoom_factor(
    const TermSize& ts, const pdf::PageSpecs& ps, const ContentArea& area, float zoom);

/**
 * @brief Returns the escape sequence that moves the cursor to wherever an
 * image should start being drawn so it appears centred within a given cell
 * area, both horizontally and vertically.
 *
 * Converts the image's pixel dimensions into the number of cells they'll
 * occupy (rounding up, since a partially-filled trailing cell still needs to
 * be reserved), then centres that many cells within content_cols x
 * content_rows, clamping the margin to zero if the image is larger than the
 * content area in either dimension (rather than moving the cursor off
 * screen or to a negative position).
 *
 * @param ts Current terminal size, used for its pixels_per_col/
 *           pixels_per_row to convert w_pixels/h_pixels into a cell count.
 * @param w_pixels Width of the image to be centred, in pixels.
 * @param h_pixels Height of the image to be centred, in pixels.
 * @param area Region the image is being centred within; start_row/start_col
 *             give the area's top-left cell (e.g. row 2 to sit just below a
 *             one-row top bar), added to the computed margin to get the
 *             final cursor position.
 * @return Escape sequence that moves the cursor to the computed
 *         top-left drawing position; does not draw anything itself.
 */
std::string center_cursor(const TermSize& ts, int w_pixels, int h_pixels, const ContentArea& area);
}  // namespace TUI
