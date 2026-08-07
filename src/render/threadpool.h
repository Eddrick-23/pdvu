#pragma once
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t n);
  ~ThreadPool();

  void worker_loop();

  template <typename F, typename... Args>
  auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using result_type = std::invoke_result_t<F, Args...>;

    // create function with bounded params
    auto bound_task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    // wrap in a shared ptr to allow copy construct/assign
    auto packaged = std::make_shared<std::packaged_task<result_type()>>(std::move(bound_task));

    std::future<result_type> fut = packaged->get_future();
    // acquire lock and enqueue packaged task
    {
      std::scoped_lock lock(queue_mutex_);
      // cannot store packaged task directly in queue
      // wrap in a lambda for type erasure
      // use mutable since task can change internal state but lambdas are const
      // by default
      if (shutdown_) {
        throw std::runtime_error("enqueue on stopped ThreadPool");
      }
      tasks_.emplace([packaged]() mutable { (*packaged)(); });
    }
    queue_cv_.notify_one();
    return fut;
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

 private:
  /**
   * @brief helper that sets flips shutdown flag, wakes all worker threads
   * and joins them.
   */
  void shutdown_and_join();
  bool shutdown_ = false;             ///< flag to track threadpool shutdown
  std::vector<std::thread> workers_;  ///< worker threads
  // tasks and synchronisation
  using Task = std::function<void()>;
  std::queue<Task> tasks_;  ///< queue containing pending tasks
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
};
