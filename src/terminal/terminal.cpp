#include "terminal.h"

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <format>
#include <print>
#include <system_error>

#include "plog/Log.h"
// set as 1 so terminal caches the dimensions on startup
volatile sig_atomic_t Terminal::window_resized = 2;
volatile sig_atomic_t Terminal::quit_requested = 0;
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
std::string move_cursor(int row, int col) { return std::format("\033[{};{}H", row, col); }
std::string_view reset_screen_and_cursor_string() {
  return "\033[H\033[J";  // avoid [2J since it deletes stored images
}
std::string_view save_cursor_string() { return "\0337"; }
std::string_view restore_cursor_string() { return "\0338"; }
}  // namespace terminal

Terminal::Terminal() = default;

Terminal::~Terminal() noexcept {
  exit_raw_mode();
  terminal::exit_alt_screen();
  terminal::show_cursor();
}

void Terminal::handle_sigwinch(int /*sig*/) { window_resized = 1; }
void Terminal::handle_sigterm(int /*sig*/) { quit_requested = 1; }

void Terminal::setup_signal_handlers() {
  struct sigaction sa_resize{};
  sa_resize.sa_handler = handle_sigwinch;
  sigemptyset(&sa_resize.sa_mask);
  // Important: Force blocking calls like read to return -1 when a signal
  // arrives
  sa_resize.sa_flags = 0;
  if (sigaction(SIGWINCH, &sa_resize, nullptr) == -1) {
    perror("sigaction");
  }
  struct sigaction sa_quit{};
  sa_quit.sa_handler = handle_sigterm;
  sigemptyset(&sa_quit.sa_mask);
  sa_quit.sa_flags = 0;
  // Register for SIGHUP(Tab Close), SIGTERM(kill), SIGINT(Ctrl-c)
  if (sigaction(SIGHUP, &sa_quit, nullptr) == -1) {
    perror("sigaction SIGHUP");
  }
  if (sigaction(SIGTERM, &sa_quit, nullptr) == -1) {
    perror("sigaction SIGTERM");
  }
  if (sigaction(SIGINT, &sa_quit, nullptr) == -1) {
    perror("sigaction SIGINT");
  }
}

bool Terminal::was_resized() {
  if (window_resized != 0) {
    get_terminal_size();  // update attributes before reset
    window_resized = 0;
    return true;
  }
  return false;
}

TermSize Terminal::get_terminal_size() {
  winsize ws{};
  if (window_resized == 0) {  // not resized, use cached
    return m_term_size;
  }
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
    const int columns = static_cast<int>(ws.ws_col);
    const int rows = static_cast<int>(ws.ws_row);
    const int pixel_width = static_cast<int>(ws.ws_xpixel);
    const int pixel_height = static_cast<int>(ws.ws_ypixel);
    // guard against divide by zero
    if (columns <= 0 || rows <= 0 || pixel_width <= 0 || pixel_height <= 0) {
      PLOG_ERROR << "Terminal reported invalid dimensions";
      return m_term_size;
    }
    // account dead spacing
    const int drawable_x_pixels = ws.ws_xpixel - (ws.ws_xpixel % ws.ws_col);
    const int drawable_y_pixels = ws.ws_ypixel - (ws.ws_ypixel % ws.ws_row);
    const int cell_pixel_width = drawable_x_pixels / ws.ws_col;
    const int cell_pixel_height = drawable_y_pixels / ws.ws_row;

    m_term_size = TermSize{
        .columns = columns,
        .rows = rows,
        .pixel_width = pixel_width,
        .pixel_height = pixel_height,
        .cell_pixel_width = cell_pixel_width,
        .cell_pixel_height = cell_pixel_height,
    };
    return m_term_size;
  }

  PLOG_ERROR << "Failed to get terminal size";
  return m_term_size;
}

void Terminal::enter_raw_mode() {
  if (m_raw_mode) {
    return;
  }

  termios original{};
  if (tcgetattr(STDIN_FILENO, &original) == -1) {
    const int error = errno;
    throw std::system_error(error, std::generic_category(), "Failed to read terminal attributes");
  };  // get original state

  termios raw = original;
  // TURN OFF: ECHO(printing), ICANON(enter key)
  raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    const int error = errno;
    throw std::system_error(error, std::generic_category(), "Failed to enable terminal raw mode");
  }

  m_orig_termios = original;
  m_raw_mode = true;
}

void Terminal::exit_raw_mode() noexcept {
  // restore original terminal state
  if (!m_raw_mode) {
    return;
  }
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig_termios) == -1) {
    std::perror("Failed to restore terminal attributes");
    return;
  }
  m_raw_mode = false;
}

InputEvent Terminal::read_input(int timeout_ms) {
  pollfd stdin_poll{
      .fd = STDIN_FILENO,
      .events = POLLIN,
      .revents = 0,
  };

  const int input_result = poll(&stdin_poll, 1, timeout_ms);
  if (input_result <= 0) {
    return InputEvent{.key = key_none};
  }

  char c;
  ssize_t nread = read(STDIN_FILENO, &c, 1);
  if (nread == -1) {
    if (errno == EINTR || errno == EAGAIN) {
      return InputEvent{.key = key_none};
    }
  }

  if (nread != 1) {
    return InputEvent{.key = key_none};
  }

  if (c == '\x1b') {                                     // escape key and arrow keys
    const int escape_result = poll(&stdin_poll, 1, 10);  // wait 10ms for next byte
    if (escape_result == 0) {
      return InputEvent{.key = key_escape};
    }
    if (escape_result > 0) {
      std::array<char, 3> seq;
      if (read(STDIN_FILENO, &seq[0], 1) != 1) return InputEvent{.key = key_escape};

      if (seq[0] == '\x08' || seq[0] == '\x7F') {
        return InputEvent{.key = key_alt_backspace};
      }
      if (seq[0] == '[') {
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return InputEvent{.key = key_escape};
        switch (seq[1]) {
          case 'A':
            return InputEvent{.key = key_up_arrow};
          case 'B':
            return InputEvent{.key = key_down_arrow};
          case 'C':
            return InputEvent{.key = key_right_arrow};
          case 'D':
            return InputEvent{.key = key_left_arrow};
        }
      }
      return InputEvent{.key = key_alt_char, .char_value = seq[0]};  // e.g. alt + f
    }
    return InputEvent{.key = key_escape};
  }

  if (c == '\x0D' || c == '\x0A') {
    return InputEvent{.key = key_enter};
  }
  if (c == '\x09') {
    return InputEvent{.key = key_tab};
  }
  if (c == '\x08' || c == '\x7F') {
    return InputEvent{.key = key_backspace};
  }
  if (c >= 1 && c <= 31) {
    return InputEvent{.key = key_ctrl_char, .char_value = c};
  }
  if (c >= 32 && c <= 126) {  // printable chars
    return InputEvent(key_char, c);
  }
  return InputEvent{.key = key_none};  // ignore other keys for now
}