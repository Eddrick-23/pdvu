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

/**
 * @brief Fixed-size pool for asynchronously executing callable tasks.
 *
 * Submitted tasks expose their result or exception through a `std::future`.
 * Destruction stops new submissions, drains all accepted tasks, and joins every
 * worker thread.
 */
class ThreadPool {
 public:
  /**
   * @brief Starts a pool with the requested number of worker threads.
   *
   * @param thread_count Number of worker threads to create.
   * @throws std::invalid_argument If `thread_count` is zero.
   * @throws std::runtime_error If a worker thread cannot be created.
   */
  explicit ThreadPool(std::size_t thread_count);

  /** @brief Drains all accepted tasks and joins every worker thread. */
  ~ThreadPool();

  /**
   * @brief Submits a callable for asynchronous execution.
   *
   * @tparam F Callable type.
   * @tparam Args Argument types passed to the callable.
   * @param callable Callable to execute.
   * @param args Arguments to bind to the callable.
   * @return A future containing the callable's result. Exceptions thrown by the
   * callable are stored in the future and rethrown by `std::future::get()`.
   * @throws std::runtime_error If the pool has begun shutting down.
   */
  template <typename F, typename... Args>
  auto submit(F&& callable, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using Result = std::invoke_result_t<F, Args...>;

    auto bound_task = std::bind(std::forward<F>(callable), std::forward<Args>(args)...);

    // std::function requires a copyable target, so share ownership of the
    // move-only packaged task with the queued lambda.
    auto packaged_task = std::make_shared<std::packaged_task<Result()>>(std::move(bound_task));

    std::future<Result> future = packaged_task->get_future();
    {
      std::scoped_lock lock(m_mutex);
      if (m_shutdown) {
        throw std::runtime_error("submit on stopped ThreadPool");
      }
      m_tasks.emplace([packaged_task] { (*packaged_task)(); });
    }
    m_condition_variable.notify_one();
    return future;
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

 private:
  /**
   * @brief Marks the pool as shutting down, drains queued tasks, and joins all
   * worker threads.
   */
  void shutdown_and_join();

  /** @brief Waits for and executes tasks until shutdown completes. */
  void worker_loop();

  using Task = std::function<void()>;

  bool m_shutdown = false;                       ///< Whether new submissions are rejected.
  std::vector<std::thread> m_workers;            ///< Worker threads owned by the pool.
  std::queue<Task> m_tasks;                      ///< Accepted tasks waiting to execute.
  std::mutex m_mutex;                            ///< Protects m_shutdown and m_tasks.
  std::condition_variable m_condition_variable;  ///< Notifies workers of tasks or shutdown.
};
