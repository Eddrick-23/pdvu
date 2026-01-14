#pragma once
#include "keys.h"
#include <string>
#include <sys/termios.h>
struct TermSize {
  int width, height;
  int x, y; // (pixels)
  int pixels_per_row, pixels_per_col;
};

namespace terminal {
void hide_cursor();
void show_cursor();
void enter_alt_screen();
void exit_alt_screen();
std::string move_cursor(int row, int col);
std::string reset_screen_and_cursor_string();
} // namespace terminal

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
  void die(const char *s);
  TermSize get_terminal_size();
  InputEvent read_input(int timeout_ms);

private:
  termios orig_termios;
  bool raw_mode = false;
  int width, height;
  int x_pixels, y_pixels;
  int drawable_x_pixels, drawable_y_pixels;
  int pixels_per_row, pixels_per_col;
};