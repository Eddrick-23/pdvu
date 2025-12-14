#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include "parser.h"
#include "shm.h"
#include "tempfile.h"

struct RenderRequest {
    int page_num;
    float zoom;
    // We use a generation ID to ignore results from old requests if needed
    size_t req_id;
    std::string transmission;
};

struct RenderResult {
    size_t req_id;
    int page_num;
    int page_width;
    int page_height;
    std::string error_message; // empty if successful
    int render_time_ms;

    std::unique_ptr<SharedMemory> shm;
    std::unique_ptr<Tempfile> tempfile;
    std::string transmission;
};

class RenderEngine {
public:
    explicit RenderEngine(const Parser& prototype_parser);
    ~RenderEngine();

    // main thread calls to request a page
    void request_page(int page_num, float zoom, const std::string& transmission);
    // main thread calls to check if a result is ready
    std::optional<RenderResult> get_result();
private:
    void worker_loop();

    // core
    std::unique_ptr<Parser> parser; // thread local parser
    std::thread worker;
    std::atomic<bool> running = true;
    std::atomic<size_t> current_req_id = 0;

    // thread safety and synchronisation
    std::mutex state_mutex; // shared between cv_worker and actual worker
    std::condition_variable cv_worker;

    // We use a single optional for input to automatically "drop" older frames
    std::optional<RenderRequest> pending_request;

    // variable to keep track of latest result;
    std::optional<RenderResult> latest_result;
};

