#pragma once
#include <sys/termios.h>

#include <csignal>
#include <string>

#include "viewer/keys.h"
struct TermSize {
  int columns;
  int rows;
  int pixel_width;
  int pixel_height;
  int cell_pixel_width;
  int cell_pixel_height;
};

namespace terminal {
void hide_cursor();
void show_cursor();
void enter_alt_screen();
void exit_alt_screen();
std::string move_cursor(int row, int col);
std::string_view reset_screen_and_cursor_string();
std::string_view save_cursor_string();
std::string_view restore_cursor_string();
}  // namespace terminal

class Terminal {
 public:
  static volatile sig_atomic_t window_resized;
  static volatile sig_atomic_t quit_requested;
  Terminal();
  ~Terminal();

  static void handle_sigwinch(int sig);
  static void handle_sigterm(int sig);
  void setup_signal_handlers();
  bool was_resized();

  void enter_raw_mode();
  void exit_raw_mode();
  void die(const char* s);
  /**
   * @brief query terminal dimensions using posix api
   * @return TermSize struct which contains drawable terminal dimensions
   * @Note Caches result if window is not resized, if query failed, we fallback to
   * the last recorded TermSize.
   */
  TermSize get_terminal_size();
  InputEvent read_input(int timeout_ms);

 private:
  termios orig_termios;
  bool raw_mode = false;  ///< Tracks if we are in raw mode
  TermSize m_term_size{
      .columns = 80,
      .rows = 24,
      .pixel_width = 0,
      .pixel_height = 0,
      .cell_pixel_width = 0,
      .cell_pixel_height = 0,
  };
};