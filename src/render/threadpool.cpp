#include "threadpool.h"

#include <cstddef>
#include <stdexcept>
#include <thread>

#include "utils/profiling.h"

ThreadPool::ThreadPool(std::size_t n) {
  ZoneScopedN("threadpool setup");
  if (n == 0) {
    throw std::invalid_argument("initialised with thread count 0");
  }

  m_workers.reserve(n);
  try {
    for (std::size_t i = 0; i < n; i++) {
      auto thread = std::thread(&ThreadPool::worker_loop, this);
      m_workers.emplace_back(std::move(thread));
    }
  } catch (...) {
    // if any thread creation throws
    // clean up any already created threads.
    shutdown_and_join();
    throw std::runtime_error("Thread creation failed during setup");
  }
}

ThreadPool::~ThreadPool() { shutdown_and_join(); }

void ThreadPool::shutdown_and_join() {
  // wrap flag update to prevent race conditions during shutdown
  {
    std::scoped_lock lock(m_mutex);
    m_shutdown = true;
  }
  m_condition_variable.notify_all();
  for (auto& t : m_workers) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void ThreadPool::worker_loop() {
  while (true) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition_variable.wait(lock, [this] { return !m_tasks.empty() || m_shutdown; });

      if (m_shutdown && m_tasks.empty()) {
        return;
      }
      task = std::move(m_tasks.front());
      m_tasks.pop();
    }
    task();  // execute task
  }
}
