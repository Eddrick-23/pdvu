#pragma once
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "parser.h"

class ThreadPool {

public:
    ThreadPool(const IParser& prototype_parser, int n);
    ~ThreadPool();

    void worker_loop(IParser& parser);

    template<typename F>
    auto enqueue_with_future(F &&f) -> std::future<std::invoke_result_t<F, IParser&> > {
        using Result = std::invoke_result_t<F, IParser&>;
        auto task = std::make_shared<std::packaged_task<Result(IParser&)>>(std::forward<F>(f));
        std::future<Result> fut = task->get_future();
        // acquire lock and enqueue
        {
            std::lock_guard<std::mutex> lock (queue_mutex_);
            // cannot store packaged task directly in queue
            // wrap in a lambda for type erasure
            // use mutable since task can change internal state but lambdas are const by default
            if (shutdown_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task = std::move(task)](IParser& parser) mutable{
                (*task)(parser);
            });
        }
        queue_cv_.notify_one();
        return fut;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
private:
    struct Worker {
        // custom wrapping of std::thread
        // holds its own duplicated parser instance
        std::thread thread;
        std::unique_ptr<IParser> parser;
    };
    // flag to indicate whether we want to close the threadpool
    bool shutdown_ = false;
    // pool of workers
    std::vector<Worker> workers_;
    // tasks and synchronisation
    using Task = std::function<void(IParser&)>;
    std::queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
};
