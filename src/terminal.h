#pragma once
#include <sys/termios.h>
#include <string>
#include "keys.h"
struct TermSize {
    int width, height;
    int x, y; // (pixels)
    int pixels_per_row, pixels_per_col;
};

class Terminal {
public:
    static volatile sig_atomic_t window_resized;
    Terminal();
    ~Terminal();

    static void handle_sigwinch(int sig);
    void setup_resize_handler();
    bool was_resized();

    void enter_raw_mode();
    void exit_raw_mode();
    void enter_alt_screen() const;
    void exit_alt_screen() const;
    void hide_cursor() const;
    void show_cursor() const;
    void die(const char *s);
    TermSize get_terminal_size();
    std::string top_bar_string(const std::string& left,
                               const std::string& mid,
                               const std::string& right);
    std::string bottom_bar_string();
    std::string move_cursor(int row, int col) const;
    std::string reset_screen_and_cursor_string();
    std::string get_input(const std::string& prompt);
    InputEvent read_input();
private:
    termios orig_termios;
    bool raw_mode = false;
    int width, height;
    int x_pixels, y_pixels;
    int pixels_per_row, pixels_per_col;
};