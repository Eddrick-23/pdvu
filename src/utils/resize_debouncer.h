#pragma once
#include <chrono>

/**
 * @brief Tracks a debounced "settled" event from a stream of raw signal ticks.
 *
 * Feed it a boolean once per loop tick (e.g. the result of
 * Terminal::was_resized()); it does not query anything itself. Useful for
 * collapsing repeated "resize started... still resizing... resizing stopped"
 * polling loops into a single call.
 */

enum class ResizeState { Idle, Resizing, Settled };

class ResizeDebouncer {
 public:
  /**
   * @brief Create a debouncer for resize events
   * @param debounce_ms How long the signal must stay quiet (false) before poll() reports Settled.
   */
  explicit ResizeDebouncer(int debounce_ms) : m_debounce_ms(debounce_ms) {}

  /**
   * @brief Advance the debouncer by one tick.
   *
   * Call this once per tick with the raw signal (e.g. Terminal::was_resized()).
   * Resets the internal timer whenever the signal is true; otherwise checks whether
   * the debounce window has elapsed since the last (true) signal
   *
   * @param resize_signal_now Whether the raw resize signal fired this tick.
   * @returns ResizeState::Resizing while a resize signal is active or debounce window has not
   * elapsed yet; ResizeState::Settled on the single tick the window elapses; ResizeState::Idle if
   * no resize has been signaled (or prior resizing has already settled)
   */
  ResizeState poll(bool resize_signal_now) {
    using namespace std::chrono;
    if (resize_signal_now) {
      m_last_signal = steady_clock::now();
      m_resizing = true;
      return ResizeState::Resizing;
    }

    // not resizing, and new signal is false
    // state is idle.
    if (!m_resizing) {
      return ResizeState::Idle;
    }

    // previously resizing, check if m_debounce_ms has elapsed
    // then resizing has settled. Else we are still resizing.
    const auto since = duration_cast<milliseconds>(steady_clock::now() - m_last_signal);
    if (since.count() > m_debounce_ms) {
      m_resizing = false;
      return ResizeState::Settled;
    }
    return ResizeState::Resizing;
  }

 private:
  int m_debounce_ms;
  bool m_resizing{false};
  std::chrono::steady_clock::time_point m_last_signal;
};