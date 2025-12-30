#pragma once
#include "terminal.h"
#include <functional>
#include <string>

namespace TUI::symbols {

inline const std::unordered_map<int, std::string_view> box_single_line = {
    {191, "\u2510"}, // ┐
    {192, "\u2514"}, // └
    {193, "\u2534"}, // ┴
    {194, "\u252C"}, // ┬
    {195, "\u251C"}, // ├
    {196, "\u2500"}, // ─
    {197, "\u253C"}, // ┼
    {217, "\u2518"}, // ┘
    {218, "\u250C"}, // ┌
    {179, "\u2502"}, // │
    {180, "\u2524"}  // ┤
};

inline const std::unordered_map<int, std::string_view> box_double_line = {
    {185, "\u2563"}, // ╣
    {186, "\u2551"}, // ║
    {187, "\u2557"}, // ╗
    {188, "\u255D"}, // ╝
    {200, "\u255A"}, // ╚
    {201, "\u2554"}, // ╔
    {202, "\u2569"}, // ╩
    {203, "\u2566"}, // ╦
    {204, "\u2560"}, // ╠
    {205, "\u2550"}, // ═
    {206, "\u256C"}  // ╬
};

} // namespace TUI::symbols
namespace TUI {
// min dimensions for displaying guard message
inline constexpr int MIN_ROWS = 40;
inline constexpr int MIN_COLS = 40;
std::string add_centered(int row, int term_width, const std::string &text,
                         int text_length);
std::string top_bar(const TermSize &ts, const std::string &left,
                    const std::string &mid, const std::string &right);
std::string bottom_bar(const TermSize &ts);
std::string guard_message(const TermSize &ts);
std::string help_overlay(const TermSize &ts);
std::string bottom_input_bar(Terminal &term, const std::string &prompt,
                             const std::function<void()> &on_resize);
} // namespace TUI
