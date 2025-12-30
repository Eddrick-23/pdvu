#include "terminal.h"
#include <cstdio>
#include <fstream>
#include <print>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
volatile sig_atomic_t Terminal::window_resized =
    1; // set as 1 so terminal caches the dimensions on startup

namespace terminal {
void hide_cursor() {
  std::print("{}", "\033[?25l");
  std::fflush(stdout);
}
void show_cursor() {
  std::print("{}", "\033[?25h");
  std::fflush(stdout);
}
void enter_alt_screen() {
  std::print("{}", "\033[?1049h");
  std::fflush(stdout);
}
void exit_alt_screen() {
  std::print("{}", "\033[?1049l");
  std::fflush(stdout);
}
std::string move_cursor(int row, int col) {
  return std::format("\033[{};{}H", row, col);
}
std::string reset_screen_and_cursor_string() { return "\033[H\033[2J"; }
} // namespace terminal

Terminal::Terminal() {}

Terminal::~Terminal() {
  exit_raw_mode();
  terminal::exit_alt_screen();
  terminal::show_cursor();
}

void Terminal::handle_sigwinch(int sig) { window_resized = 1; }

void Terminal::setup_resize_handler() {
  struct sigaction action;
  action.sa_handler = handle_sigwinch;
  sigemptyset(&action.sa_mask);

  // Force blocking calls like read to return -1 when a signal arrives
  action.sa_flags = 0;

  if (sigaction(SIGWINCH, &action, NULL) == -1) {
    perror("sigaction");
  }
}

bool Terminal::was_resized() {
  if (window_resized) {
    get_terminal_size(); // update attributes before reset
    window_resized = 0;
    return true;
  }
  return false;
}

TermSize Terminal::get_terminal_size() {
  winsize ws{};
  if (window_resized == 0) { // not resized, use cached
    return TermSize(width, height, x_pixels, y_pixels, pixels_per_row,
                    pixels_per_col);
  }
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
    width = ws.ws_col;
    height = ws.ws_row;
    x_pixels = ws.ws_xpixel;
    y_pixels = ws.ws_ypixel;
    // account dead spacing
    drawable_x_pixels = ws.ws_xpixel - (ws.ws_xpixel % ws.ws_col);
    drawable_y_pixels = ws.ws_ypixel - (ws.ws_ypixel % ws.ws_row);
    pixels_per_row = drawable_y_pixels / ws.ws_row;
    pixels_per_col = drawable_x_pixels / ws.ws_col;

    return TermSize(ws.ws_col, ws.ws_row, ws.ws_xpixel, ws.ws_ypixel,
                    pixels_per_row, pixels_per_col);
  }
  std::println(stderr, "Failed to get terminal size");
  return TermSize(24, 80, 0, 0, 0, 0);
}

void Terminal::enter_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tctgetattr");
  }; // get original state

  struct termios raw = orig_termios;
  // TURN OFF: ECHO(printing), ICANON(enter key), ISIG(ctrl-c/z signals)
  // Keep ISIG for debugging
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tctsetattr");
  }
  raw_mode = true;
}

void Terminal::exit_raw_mode() {
  // restore original terminal state
  if (!raw_mode) {
    return;
  }
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tctsetattr");
  }
}

void Terminal::die(const char *s) {
  // cleanup
  exit_raw_mode();
  terminal::exit_alt_screen();
  terminal::show_cursor();
  // print error message
  perror(s);
  exit(1);
}

InputEvent Terminal::read_input(int timeout_ms) {
  pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;

  int ret = poll(&pfd, 1, timeout_ms);
  if (ret <= 0) {
    return InputEvent{key_none};
  }

  char c;
  int nread = read(STDIN_FILENO, &c, 1);
  if (nread == -1) {
    if (errno == EINTR) {
      return InputEvent{key_none};
    }
    if (errno == EAGAIN) {
      die("read");
    }
    return InputEvent{key_none};
  }

  if (nread != 1) {
    return InputEvent{key_none};
  }

  if (c == '\x1b') { // escape key and arrow keys
    pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 10); // wait 10ms for next byte
    if (ret == 0) {
      return InputEvent{key_escape};
    }
    if (ret > 0) {
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1)
        return InputEvent{key_escape};

      if (seq[0] == '\x08' || seq[0] == '\x7F') {
        return InputEvent{key_alt_backspace};
      }
      if (seq[0] == '[') {
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
          return InputEvent{key_escape};
        switch (seq[1]) {
        case 'A':
          return InputEvent{key_up_arrow};
        case 'B':
          return InputEvent{key_down_arrow};
        case 'C':
          return InputEvent{key_right_arrow};
        case 'D':
          return InputEvent{key_left_arrow};
        }
      }
      return InputEvent{key_alt_char, seq[0]}; // e.g. alt + f
    }
    return InputEvent{key_escape};
  }

  if (c == '\x0D' || c == '\x0A') {
    return InputEvent{key_enter};
  }
  if (c == '\x09') {
    return InputEvent{key_tab};
  }
  if (c == '\x08' || c == '\x7F') {
    return InputEvent{key_backspace};
  }
  if (c >= 1 && c <= 31) {
    return InputEvent{key_ctrl_char, c};
  }
  if (c >= 32 && c <= 126) { // printable chars
    return InputEvent(key_char, c);
  }
  return InputEvent{key_none}; // ignore other keys for now
}