#pragma once
#include <sys/termios.h>
#include <string>

struct TermSize {
    int width, height;
    int x, y; // (pixels)
    int pixels_per_row, pixels_per_col;
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    void enter_raw_mode();
    void exit_raw_mode();
    void enter_alt_screen() const;
    void exit_alt_screen() const;
    void hide_cursor() const;
    void show_cursor() const;
    void die(const char *s);
    TermSize get_terminal_size() const;
    std::string top_bar_string(const std::string& left,
                               const std::string& mid,
                               const std::string& right);
    std::string bottom_bar_string();
    std::string move_cursor(int row, int col) const;
    std::string reset_screen_and_cursor_string();
private:
    struct termios orig_termios;
    bool raw_mode = false;
};