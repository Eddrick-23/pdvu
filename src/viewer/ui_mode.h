#pragma once
#include <utility>

namespace viewer {
/**
 * @brief Identifies the active UI mode and determines input routing and drawing.
 */
enum class UiMode {
  Browse,    ///< Normal document controls
  GoToPage,  ///< Page-number entry through TUI::InputBar
  Help,      ///< Viewing help ui page
};

struct ModePresentation {
  bool redraw_on_page_frame;
  bool show_bottom_status_bar;
  bool show_input_cursor;
};

[[nodiscard]] inline ModePresentation presentation_for(UiMode mode) {
  switch (mode) {
    case UiMode::Browse:
      return {
          .redraw_on_page_frame = true,
          .show_bottom_status_bar = true,
          .show_input_cursor = false,
      };
    case UiMode::GoToPage:
      return {
          .redraw_on_page_frame = true,
          .show_bottom_status_bar = false,
          .show_input_cursor = true,
      };
    case UiMode::Help:
      return {
          .redraw_on_page_frame = false,
          .show_bottom_status_bar = false,
          .show_input_cursor = false,
      };
  }
  std::unreachable();
}
}  // namespace viewer
