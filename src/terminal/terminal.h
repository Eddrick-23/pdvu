#pragma once
#include <sys/termios.h>

#include <csignal>
#include <string>
#include <string_view>

#include "viewer/keys.h"

/**
 * @brief Snapshot of the terminal's text and pixel dimensions.
 *
 * Cell dimensions are integral pixel approximations derived from the complete
 * terminal dimensions.
 */
struct TermSize {
  int columns;            ///< Number of text columns.
  int rows;               ///< Number of text rows.
  int pixel_width;        ///< Total terminal width in pixels.
  int pixel_height;       ///< Total terminal height in pixels.
  int cell_pixel_width;   ///< Approximate width of one terminal cell in pixels.
  int cell_pixel_height;  ///< Approximate height of one terminal cell in pixels.
};

namespace terminal {
/// Immediately hides the terminal cursor.
void hide_cursor();

/// Immediately makes the terminal cursor visible.
void show_cursor();

/// Switches to the terminal's alternate screen buffer.
void enter_alt_screen();

/// Returns to the terminal's primary screen buffer.
void exit_alt_screen();

/**
 * @brief Creates an ANSI sequence that moves the cursor.
 * @param row One-based terminal row.
 * @param col One-based terminal column.
 */
[[nodiscard]] std::string move_cursor(int row, int col);

/// Returns an ANSI sequence that moves home and clears the visible screen.
[[nodiscard]] std::string_view reset_screen_and_cursor_string();

/// Returns an ANSI sequence that saves the current cursor position.
[[nodiscard]] std::string_view save_cursor_string();

/// Returns an ANSI sequence that restores the saved cursor position.
[[nodiscard]] std::string_view restore_cursor_string();
}  // namespace terminal

/**
 * @brief Manages POSIX terminal input mode, signals, and cached dimensions.
 *
 * Raw input mode is entered explicitly with enter_raw_mode(). If enabled, the
 * original terminal attributes are restored during destruction. The class is
 * non-copyable because restoration state must have a single owner.
 */
class Terminal {
 public:
  static volatile sig_atomic_t window_resized;
  static volatile sig_atomic_t quit_requested;
  Terminal();
  ~Terminal() noexcept;

  static void handle_sigwinch(int sig);
  static void handle_sigterm(int sig);

  /**
   * @brief Installs process-wide resize and termination signal handlers.
   *
   * Existing handlers for SIGWINCH, SIGHUP, SIGTERM, and SIGINT are replaced.
   * Registration failures are reported to stderr.
   */
  void setup_signal_handlers();

  /**
   * @brief Consumes the pending resize notification.
   *
   * Refreshes the cached dimensions before clearing the resize flag.
   *
   * @return true if a resize was pending; otherwise false.
   */
  bool was_resized();

  /**
   * @brief Enables noncanonical, non-echoing terminal input.
   *
   * Repeated calls have no effect while the mode is already active. The original
   * attributes are retained for restoration during destruction.
   *
   * @throws std::system_error If terminal attributes cannot be read or changed.
   */
  void enter_raw_mode();

  /**
   * @brief Returns the current terminal dimensions.
   *
   * Queries the terminal after a resize; otherwise returns the cached value.
   * Query failures leave the cached value unchanged.
   */
  TermSize get_terminal_size();

  /**
   * @brief Waits for and decodes one terminal input event.
   *
   * @param timeout_ms Poll timeout in milliseconds. Zero performs a nonblocking
   * check; a negative value waits indefinitely.
   * @return The decoded event, or key_none on timeout, interruption, or unsupported input.
   */
  InputEvent read_input(int timeout_ms);

  Terminal(const Terminal&) = delete;
  Terminal& operator=(const Terminal&) = delete;
  Terminal(Terminal&&) = delete;
  Terminal& operator=(Terminal&&) = delete;

 private:
  /**
   * @brief Restores the terminal attributes saved before entering raw mode.
   *
   * Has no effect if raw mode is not active. Restoration failures are reported
   * to stderr and leave the terminal marked as being in raw mode so cleanup may
   * be attempted again.
   *
   * @note This function never throws.
   */
  void exit_raw_mode() noexcept;

  termios m_orig_termios;   ///< Stores original terminal attributes to restore later
  bool m_raw_mode = false;  ///< Tracks if we are in raw mode
  TermSize m_term_size{
      .columns = 80,
      .rows = 24,
      .pixel_width = 0,
      .pixel_height = 0,
      .cell_pixel_width = 0,
      .cell_pixel_height = 0,
  };
};