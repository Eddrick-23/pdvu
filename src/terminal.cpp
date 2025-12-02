#include "terminal.h"
#include <iostream>
#include <memory>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

Terminal::Terminal() {
    enter_raw_mode();
}

Terminal::~Terminal() {
    //exit raw mode
    exit_raw_mode();
    show_cursor();

}


TermSize Terminal::get_terminal_size() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        return TermSize(ws.ws_col, ws.ws_row, ws.ws_xpixel, ws.ws_ypixel);
    } else {
        std::cerr << "Failed to get terminal size" << std::endl;
        return TermSize(24, 80, 0, 0);
    }
}

void Terminal::enter_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios); // get original state

    struct termios raw = orig_termios;
    // TURN OFF: ECHO(printing), ICANON(enter key), ISIG(ctrl-c/z signals)
    // Keep ISIG for debugging
    raw.c_lflag &= ~(ECHO | ICANON);

    raw_mode = true;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void Terminal::exit_raw_mode() const {
    // restore original terminal state
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}

void Terminal::hide_cursor() const {
    std::cout << "\033[?25l" << std::flush;
}

void Terminal::show_cursor() const {
    std::cout << "\033[?25h" << std::flush;
}

void Terminal::clear_terminal() const {
    std::cout << "\033[2J\033[H"; // clear terminal
}
