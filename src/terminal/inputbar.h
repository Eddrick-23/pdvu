#pragma once
#include <string>

#include "terminal.h"
#include "viewer/keys.h"

namespace TUI {
enum class InputBarAction {
  None,
  Changed,
  Submitted,
  Cancelled,
};

class InputBar {
 public:
  explicit InputBar(std::string_view prompt);

  InputBarAction handle(const InputEvent& event);
  std::string render_sequence(const TermSize& ts);

  [[nodiscard]] std::string_view value() const;

  void set_error(std::string error);
  void clear_error();
  void reset();

 private:
  std::string m_prompt;
  std::string m_buffer;
  std::string m_error_prompt;

  std::size_t m_cursor_pos = 0;
  std::size_t m_visible_pos = 0;
};
}  // namespace TUI