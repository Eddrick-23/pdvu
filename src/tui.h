#pragma once
#include <string>
#include "terminal.h"

namespace TUI {
    // min dimensions for displaying guard message
    inline constexpr int MIN_ROWS = 23;
    inline constexpr int MIN_COLS = 40;
    std::string top_bar(const TermSize& ts, const std::string& left, const std::string& mid, const std::string& right);
    std::string bottom_bar(const TermSize& ts);
    std::string guard_message(const TermSize& ts);
    std::string help_overlay();
}
