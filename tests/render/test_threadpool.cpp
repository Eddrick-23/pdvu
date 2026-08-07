#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>

#include "render/threadpool.h"

TEST(ThreadPoolTest, ThrowsOnZeroInput) { EXPECT_THROW(ThreadPool(0), std::invalid_argument); }

TEST(ThreadPoolTest, ReturnsValuesThroughFutures) {
  auto pool = ThreadPool(1);
  auto fut = pool.submit([]() { return 10; });

  int res = 0;
  ASSERT_NO_THROW(res = fut.get());
  EXPECT_EQ(res, 10);
}

TEST(ThreadPoolTest, ExceptionsPropagateThroughFutures) {
  auto pool = ThreadPool(1);
  auto fut = pool.submit([]() { throw std::runtime_error("error in task"); });

  ASSERT_THROW(fut.get(), std::runtime_error);
}

TEST(ThreadPoolTest, ExecuteMultipleQueuedTasks) {
  constexpr int task_count = 5;
  auto pool = ThreadPool(1);
  std::vector<std::future<int>> futures;
  futures.reserve(task_count);
  for (int i = 0; i < task_count; i++) {
    futures.push_back(pool.submit([i]() { return i; }));
  }

  for (int i = 0; i < task_count; i++) {
    int res;
    ASSERT_NO_THROW(res = futures[static_cast<std::size_t>(i)].get());
    EXPECT_EQ(res, i);
  }
}

TEST(ThreadPoolTest, DestructorDrainsAcceptedTasks) {
  using namespace std::chrono_literals;
  constexpr int task_count = 8;
  std::vector<std::future<int>> futures;
  futures.reserve(task_count);

  {
    ThreadPool pool{1};

    for (int i = 0; i < task_count; ++i) {
      futures.push_back(pool.submit([i] { return i; }));
    }
  }  // Pool destruction must drain every accepted task.
  for (int i = 0; i < task_count; ++i) {
    auto& future = futures[static_cast<std::size_t>(i)];
    // all tasks should be drained so we should not need to wait
    ASSERT_EQ(future.wait_for(0s), std::future_status::ready);
    EXPECT_EQ(future.get(), i);
  }
}

TEST(ThreadPoolTest, MultipleWorkersExecuteConcurrently) {
  using namespace std::chrono_literals;
  ThreadPool pool{2};

  std::mutex mutex;
  std::condition_variable cv;
  int started_tasks = 0;
  bool release_tasks = false;

  auto blocking_task = [&] {
    std::unique_lock lock(mutex);

    ++started_tasks;
    cv.notify_all();

    cv.wait(lock, [&] { return release_tasks; });
  };

  auto first = pool.submit(blocking_task);
  auto second = pool.submit(blocking_task);

  bool both_started = false;
  {
    std::unique_lock lock(mutex);

    both_started = cv.wait_for(lock, 5s, [&] { return started_tasks == 2; });

    // Always release tasks, including when the assertion will fail.
    release_tasks = true;
  }
  cv.notify_all();

  EXPECT_TRUE(both_started);
  EXPECT_NO_THROW(first.get());
  EXPECT_NO_THROW(second.get());
}