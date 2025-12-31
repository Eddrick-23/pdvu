#include "threadpool.h"
#include "utils/profiling.h"

ThreadPool::ThreadPool(const pdf::Parser &prototype_parser, const int n) {
  ZoneScopedN("threadpool setup");
  for (int i = 0; i < n; i++) {
    // use fresh parser classes so they don't share caches
    auto worker_parser = prototype_parser.duplicate();
    auto thread =
        std::thread(&ThreadPool::worker_loop, this, std::ref(*worker_parser));

    workers_.emplace_back(std::move(thread), std::move(worker_parser));
  }
}

ThreadPool::~ThreadPool() {
  shutdown_ = true;
  queue_cv_.notify_all();
  for (auto &[thread, parser] : workers_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void ThreadPool::worker_loop(pdf::Parser &parser) {
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
    task(parser); // execute task
  }
}
