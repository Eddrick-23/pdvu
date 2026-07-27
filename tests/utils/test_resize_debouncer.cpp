#include <gtest/gtest.h>
#include <utils/resize_debouncer.h>

TEST(ResizeDebouncer, IdleOnInitialPoll) {
  constexpr auto start = std::chrono::steady_clock::time_point{};
  auto debouncer = ResizeDebouncer(100);
  EXPECT_EQ(ResizeState::Idle, debouncer.poll(false, start));
}

TEST(ResizeDebouncer, ResizingOnFirstTruePoll) {
  constexpr auto start = std::chrono::steady_clock::time_point{};
  auto debouncer = ResizeDebouncer(100);
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(true, start));
}

TEST(ResizeDebouncer, SettledAfterDebounceDuration) {
  using namespace std::chrono;
  constexpr auto start = steady_clock::time_point{};
  constexpr int debounce_ms = 100;
  auto debouncer = ResizeDebouncer(debounce_ms);
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(true, start));
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(false, start));

  // time has passed, should now settle
  EXPECT_EQ(ResizeState::Settled, debouncer.poll(false, start + milliseconds(debounce_ms + 10)));

  // Subsequent calls return Idle
  EXPECT_EQ(ResizeState::Idle, debouncer.poll(false, start + milliseconds(debounce_ms + 10)));
}

TEST(ResizeDebouncer, ResizeSignalExtendsDebounce) {
  using namespace std::chrono;
  constexpr auto start = steady_clock::time_point{};
  constexpr int debounce_ms = 100;
  auto debouncer = ResizeDebouncer(debounce_ms);
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(true, start));

  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(false, start + milliseconds(50)));

  // should reset timer back to zero
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(true, start + milliseconds(50)));

  // should be 110ms > 100ms since first true, but previous poll extended the timer
  // so still resizing
  EXPECT_EQ(ResizeState::Resizing, debouncer.poll(false, start + milliseconds(110)));

  // 200ms since start and 150ms since reset
  EXPECT_EQ(ResizeState::Settled, debouncer.poll(false, start + milliseconds(200)));
  EXPECT_EQ(ResizeState::Idle, debouncer.poll(false, start + milliseconds(200)));
}