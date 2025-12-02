#pragma once
#include <sys/termios.h>

struct TermSize {
    int width, height;
    int x, y; // (pixels)
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    TermSize get_terminal_size();

    void enter_raw_mode();
    void exit_raw_mode() const;
    void hide_cursor() const;
    void show_cursor() const;
    void clear_terminal() const;
private:
    struct termios orig_termios;
    bool raw_mode = false;
};