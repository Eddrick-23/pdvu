#include "terminal.h"
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fstream>

Terminal::Terminal() {
}

Terminal::~Terminal() {
    //exit raw mode
    exit_raw_mode();
    exit_alt_screen();
    show_cursor();

}


TermSize Terminal::get_terminal_size() const {
    winsize ws{};
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        const int ppr = ws.ws_ypixel / ws.ws_row;
        const int ppc = ws.ws_xpixel / ws.ws_col;
        return TermSize(ws.ws_col, ws.ws_row, ws.ws_xpixel, ws.ws_ypixel, ppr, ppc);
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

