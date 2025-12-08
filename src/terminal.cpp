#include "terminal.h"
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fstream>
#include <signal.h>
#include <sys/poll.h>
volatile sig_atomic_t Terminal::window_resized = 1;

Terminal::Terminal() {

}

Terminal::~Terminal() {
    //exit raw mode
    exit_raw_mode();
    exit_alt_screen();
    show_cursor();

}

void Terminal::handle_sigwinch(int sig) {
    window_resized = 1;
}

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
    if (window_resized == 0) { // not resized, used cached
        return TermSize(width, height, x_pixels, y_pixels, pixels_per_row, pixels_per_col);
    }
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        width = ws.ws_col;
        height = ws.ws_row;
        x_pixels = ws.ws_xpixel;
        y_pixels = ws.ws_ypixel;
        pixels_per_row = ws.ws_ypixel / ws.ws_row;
        pixels_per_col = ws.ws_xpixel / ws.ws_col;
        const int rem_y = ws.ws_ypixel % ws.ws_row;
        if (rem_y + ws.ws_row < pixels_per_row - 1) { // prevent bottom row cutting
            pixels_per_row--;
        }
        return TermSize(ws.ws_col, ws.ws_row, ws.ws_xpixel, ws.ws_ypixel, pixels_per_row, pixels_per_col);
    } else {
        std::cerr << "Failed to get terminal size" << std::endl;
        return TermSize(24, 80, 0, 0, 0, 0);
    }
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

void Terminal::hide_cursor() const {
    std::cout << "\033[?25l" << std::flush;
}

void Terminal::show_cursor() const {
    std::cout << "\033[?25h" << std::flush;
}

std::string Terminal::move_cursor(const int row, const int col) const {
    return std::format("\033[{};{}H", row, col);
}

void Terminal::enter_alt_screen() const{
    std::cout << "\033[?1049h" << std::flush;
}

void Terminal::exit_alt_screen() const{
    std::cout << "\033[?1049l" << std::flush;
}

void Terminal::die(const char *s) {
    // clear_terminal();
    std::cout << reset_screen_and_cursor_string() << std::flush;
    perror(s);
    exit(1);
}

std::string Terminal::reset_screen_and_cursor_string() {
    return "\033[H\033[2J";
}



std::string Terminal::top_bar_string(const std::string& left, const std::string& mid, const std::string& right) {
    TermSize ts = get_terminal_size();
    std::string result;
    result += move_cursor(1, 1);
    result +=  "\033[2K\033[7m"; // invert colours

    // each char takes a col, calculate indent
    // split to three regions
    // " "###" "###" "###" "
    int region_width = (ts.width - 4) / 3;
    auto truncate = [region_width](const std::string& s) {
        if (s.length() > region_width) {
            return s.substr(0, region_width - 3) + "...";
        }
        return s;
    };
    const std::string left_text = truncate(left);
    const std::string mid_text = truncate(mid);
    const std::string right_text = truncate(right);

    // add left text
    result += " " + left_text + std::string(region_width - left_text.length(), ' ');

    // add middle text
    int mid_pad_left = (region_width - mid_text.length()) / 2;
    int mid_pad_right = region_width - mid_text.length() - mid_pad_left;
    result += " " + std::string(mid_pad_left, ' ') + mid_text + std::string(mid_pad_right, ' ');

    // add right text
    result += " " + std::string(region_width - right_text.length(), ' ') + right_text + " ";

    result += "\033[0m"; // reset colours
    return result;
}

std::string Terminal::bottom_bar_string() {
    TermSize ts = get_terminal_size();
    const std::string text = " SEARCH: ctrl/cmd + f | NAVIGATE: <- -> | QUIT : q | Help: ?";
    int padding = ts.width - text.length();
    if (padding < 0) padding = 0;

    std::string result;
    result += move_cursor(ts.height, 1);  // move to last row
    result += "\033[2K\033[7m"; // clear line and invert colours
    result += text; // text for bottom bar
    result += std::string(padding, ' ');
    result += "\033[0m"; // Reset colors
    return result;
}

std::string Terminal::get_input(const std::string& prompt) {
    /* creates a bottom bar text input UI that takes user input */
    TermSize ts = get_terminal_size();
    show_cursor();
    std::string buffer;

    int available_width = ts.width - prompt.length();
    available_width = available_width < 1 ? 1 : available_width;
    size_t cursor_pos = 0; // cursor index in the buffer
    size_t visible_pos = 0; // index of first visible char in buffer

    auto redraw = [&]() { // helper to redraw the whole line on input
        if (cursor_pos < visible_pos) { // scrolled left off screen
            visible_pos = cursor_pos;
        } else if (cursor_pos >= visible_pos + available_width) { // scrolled right off screen
            visible_pos = cursor_pos - available_width + 1;
        }

        // If buffer is longer than screen, always show the rightmost portion
        if (buffer.length() > available_width) {
            visible_pos = buffer.length() - available_width;
        }
        // clear line and draw prompt
        std::cout << move_cursor(ts.height, 1);
        std::cout << "\033[2K\033[7m"; // colour invert

        std::cout << prompt << buffer.substr(visible_pos) << std::flush;

        // move to proper position on screen
        size_t screen_col = cursor_pos - visible_pos + prompt.length() + 1;
        std::cout << move_cursor(ts.height, screen_col) << std::flush;
    };
    InputEvent c;
    redraw();
    while (true) {
        c = read_input(100);
        // break when we get esc and clear buffer
        if (c.key == key_escape) {
            buffer.clear();
            break ;
        }
        // break when we get enter
        if (c.key == key_enter) {
            break;
        }
        if (c.key == key_backspace) {
            if (!buffer.empty()) {
                buffer.erase(cursor_pos - 1, cursor_pos > 0 ? 1 : 0);
                cursor_pos--;
                redraw();
            }
        } else if (c.key == key_alt_backspace) {
            if (!buffer.empty()) { // clear all input
                buffer = buffer.substr(cursor_pos);
                cursor_pos = 0;
                visible_pos = 0;
                redraw();
            }
        } else if (c.key == key_char) { // printable chars
            buffer.insert(cursor_pos, 1, c.char_value);
            cursor_pos++;
            redraw();
        } else if (c.key == key_right_arrow) {
            if (cursor_pos < buffer.length()) {
                cursor_pos++;
                redraw();
            }
        } else if (c.key == key_left_arrow) {
            if (cursor_pos > 0) {
                cursor_pos--;
                redraw();
            }
        }
    }

    // reset colours, hide cursor and return buffer
    std::cout << "\033[0m";
    hide_cursor();
    return buffer;
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
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return InputEvent{key_escape};

            if (seq[0] == '\x08' || seq[0] == '\x7F') {
                return InputEvent{key_alt_backspace};
            }
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) != 1) return InputEvent{key_escape};
                switch (seq[1]) {
                    case 'A' : return InputEvent{key_up_arrow};
                    case 'B' : return InputEvent{key_down_arrow};
                    case 'C' : return InputEvent{key_right_arrow};
                    case 'D' : return InputEvent{key_left_arrow};
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