#include "render_engine.h"
#include "kitty.h"

RenderEngine::RenderEngine(const pdf::Parser& prototype_parser, int n_threads) {
    // parser created first because during shutdown, any context from parser must be cleared after threadpool shutdown
    parser = prototype_parser.duplicate();
    worker = std::thread(&RenderEngine::coordinator_loop, this);
    n_threads_ = n_threads;
    thread_pool = std::make_unique<ThreadPool>(prototype_parser, n_threads);
}

RenderEngine::~RenderEngine() {
    running = false;
    cv_worker.notify_all();
    if (worker.joinable()) worker.join(); // join back to main loop
}

void RenderEngine::request_page(int page_num, float zoom, const std::string& transmission) {
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        ++current_req_id;
        pending_request = RenderRequest{
            page_num, zoom, current_req_id, transmission
        };
    }
    cv_worker.notify_one(); // wake worker to render page
}

std::optional<RenderResult> RenderEngine::get_result() { // get the most recently created image
    std::lock_guard<std::mutex> lock(state_mutex);
    return std::move(latest_result);
}

// void RenderEngine::worker_loop() {
//     /* This function runs throughout the worker thread's lifetime
//      * The thread will be idle and will wait for the condition variable to wake it up
//      * The condition variable wakes the thread up when we have a new request
//      * or we stop running(exit program)
//      * Upon processsing the request, we do zero copy to shm or tempfile
//      * we store the transmission object to pass on to viewer class
//      * latest_result attribute stores the latest rendered frame
//      */
//     // TODO find a way to render to the same buffer in parallel with multiple threads
//     // first move the page parsing to a separate function?
//     while (running) {
//         RenderRequest req;
//         // wait for work
//         {
//             std::unique_lock<std::mutex> lock(state_mutex);
//             cv_worker.wait(lock ,[this] {
//                 return pending_request.has_value() || !running;
//             });
//
//             if (!running) break;
//
//             req = std::move(pending_request.value());
//             pending_request.reset();
//         }
//
//         // TODO CHECK if obsolete
//         // optimisation: if a newer request came in while waking up, process that new request
//         // check if current_req_id changed etc. but current logic works
//
//         // render the image to memory or tempfile
//         auto start = std::chrono::steady_clock::now();
//         RenderResult result;
//         result.req_id = req.req_id;
//         result.page_num = req.page_num;
//         try {
//             PageSpecs ps = parser->page_specs(req.page_num, req.zoom);
//             if (req.transmission == "shm") {
//                 auto new_shm = std::make_unique<SharedMemory>(ps.size);
//                 current_shm = std::move(new_shm);
//                 parser->write_page(req.page_num, ps.width, ps.height, req.zoom, 0,
//                                     static_cast<unsigned char*>(current_shm->data()), ps.size);
//                 result.path_to_data = current_shm->name();
//             } else { // tempfile
//                 auto new_temp = std::make_unique<Tempfile>(ps.size);
//                 current_tempfile = std::move(new_temp);
//                 parser->write_page(req.page_num, ps.width, ps.height, req.zoom, 0,
//                                     static_cast<unsigned char*>(current_tempfile->data()), ps.size);
//                 result.path_to_data = current_tempfile->path();
//             }
//             result.page_width = ps.width;
//             result.page_height = ps.height;
//             result.transmission = req.transmission;
//             auto end = std::chrono::steady_clock::now();
//             auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//             result.render_time_ms = duration.count();
//
//         } catch (const std::exception& e) {
//             result.error_message = e.what();
//         }
//         // update latest_result as newest rendered frame
//         {
//             std::lock_guard<std::mutex> lock(state_mutex);
//             latest_result = std::move(result);
//         }
//     }
// }

void RenderEngine::coordinator_loop() {
    /* Main job is to wake on new request, then break down and equeue tasks to the threadpool to execute
     *
     */

    while (running) {
        RenderRequest req;
        // wait for work
        {
            std::unique_lock<std::mutex> lock(state_mutex);
            cv_worker.wait(lock ,[this] {
                return pending_request.has_value() || !running;
            });

            if (!running) break;

            req = std::move(pending_request.value());
            pending_request.reset();
        }
        dispatch_page_write(req);
    }
}

void RenderEngine::dispatch_page_write(const RenderRequest& req) {
        auto start = std::chrono::steady_clock::now();
        RenderResult result;
        result.req_id = req.req_id;
        result.page_num = req.page_num;

        // prepare data then enqueue to threadpool
        try {
            auto dlist = parser->get_display_list(req.page_num);
            if (!dlist) {
                result.error_message = "Failed to generate display list";
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    latest_result = std::move(result);
                }
                return;
            }
            pdf::PageSpecs ps = parser->page_specs(req.page_num, req.zoom);
            auto bounds = parser->split_bounds(ps, n_threads_);
            std::vector<std::future<void>> futures;
            if (req.transmission == "shm") {
                auto new_shm = std::make_unique<SharedMemory>(ps.size);
                current_shm = std::move(new_shm);
                for (auto h_bound : bounds) {
                    auto fut = thread_pool->enqueue_with_future(
                    [this, h_bound, req, dlist](pdf::Parser& parser) mutable  {
                        parser.write_section(req.page_num, h_bound.width, h_bound.height, req.zoom, 0, dlist,
                            static_cast<unsigned char*>(current_shm->data()) + h_bound.offset, h_bound.rect);
                    });
                    futures.push_back(std::move(fut));
                }
                result.path_to_data = current_shm->name();
            } else { // tempfile
                auto new_temp = std::make_unique<Tempfile>(ps.size);
                current_tempfile = std::move(new_temp);
                for (auto h_bound : bounds) {
                    auto fut = thread_pool->enqueue_with_future(
                    [this, h_bound, req, dlist](pdf::Parser& parser) mutable  {
                        parser.write_section(req.page_num, h_bound.width, h_bound.height, req.zoom, 0, dlist,
                            static_cast<unsigned char*>(current_tempfile->data()) + h_bound.offset, h_bound.rect);
                    });
                    futures.push_back(std::move(fut));
                }
                result.path_to_data = current_tempfile->path();
            }
            // wait for future, then update result
            for (auto& fut : futures) {
                fut.get();
            }

            result.page_width = ps.width;
            result.page_height = ps.height;
            result.transmission = req.transmission;
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            result.render_time_ms = duration.count();
        } catch (const std::exception& e) {
            result.error_message = e.what();
        }
        // update frame
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            latest_result = std::move(result);
        }
}

// void RenderEngine::fetch_display_list(int page_num) {
//
// }