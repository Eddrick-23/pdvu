#pragma once
#include <string_view>

/// Control colour of text printed to terminal
namespace TermColor {
// Standard ANSI
inline constexpr std::string_view RedBg = "\x1b[1;31m";
inline constexpr std::string_view GreenBg = "\x1b[1;32m";
inline constexpr std::string_view InvertedBg = "\033[2K\033[7m";
inline constexpr std::string_view Reset = "\x1b[0m";

// 256-colour palatte
inline constexpr std::string_view OrangeFg = "\x1b[38;5;214m";
inline constexpr std::string_view WhiteFg = "\x1b[38;5;255m";
inline constexpr std::string_view BlackBg = "\x1b[48;5;16m";
}  // namespace TermColor

/// Control text formatting when printed to terminal
namespace TermText {
inline constexpr std::string_view BoldText = "\x1b[1m";
inline constexpr std::string_view ResetBold = "\x1b[22m";
}  // namespace TermText