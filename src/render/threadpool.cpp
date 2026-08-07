#include "threadpool.h"

#include <cstddef>
#include <thread>

#include "utils/profiling.h"

ThreadPool::ThreadPool(std::size_t n) {
  ZoneScopedN("threadpool setup");
  if (n == 0) {
    throw std::invalid_argument("initialised with thread count 0");
  }

  workers_.reserve(n);
  try {
    for (std::size_t i = 0; i < n; i++) {
      auto thread = std::thread(&ThreadPool::worker_loop, this);
      workers_.emplace_back(std::move(thread));
    }
  } catch (...) {
    // if any thread creation throws
    // clean up any already created threads.
    shutdown_and_join();
  }
}

ThreadPool::~ThreadPool() { shutdown_and_join(); }

void ThreadPool::shutdown_and_join() {
  // wrap flag update to prevent race conditions during shutdown
  {
    std::scoped_lock lock(queue_mutex_);
    shutdown_ = true;
  }
  queue_cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void ThreadPool::worker_loop() {
  while (true) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] { return !tasks_.empty() || shutdown_; });

      if (shutdown_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();  // execute task
  }
}
