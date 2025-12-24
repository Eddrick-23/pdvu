#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include "parser.h"
#include "utils/shm.h"
#include "utils/tempfile.h"
#include "threadpool.h"
#include "utils/lru_cache.h"
#include <cmath>

struct PageDetails {
    int page_num;
    float zoom;
    bool operator==(const PageDetails& other) const {
        constexpr float rel_eps = 1e-9;
        return page_num == other.page_num &&
               std::fabs(other.zoom - zoom) <= rel_eps * std::max(other.zoom, zoom);
    }
};

struct PageCacheData {
    std::string transmission;
    // TODO store the bytes in a raw vector and memcpy into it
    // shm is destroyed after terminal reads from it so we can't cache the pointer
    // size_t size; // store size, we will memcpy to the buffer for both tempfile and shm
    // std::shared_ptr<SharedMemory> shm_data;
    std::vector<unsigned char> shm_buffer;
    std::shared_ptr<Tempfile> tempfile_data;
    int page_width;
    int page_height;
};

struct RenderRequest {
    int page_num;
    float zoom;
    pdf::PageSpecs scaled_page_specs;
    // We use a generation ID to ignore results from old requests if needed
    size_t req_id;
    std::string transmission;
};

struct RenderResult {
    size_t req_id;
    int page_num;
    float zoom;
    int page_width;
    int page_height;
    std::string error_message; // empty if successful
    int render_time_ms;
    std::string path_to_data;
    std::string transmission;
};

class RenderEngine {
public:
    explicit RenderEngine(const pdf::Parser& prototype_parser, int n_threads);
    ~RenderEngine();

    // main thread calls to request a page
    void request_page(int page_num, float zoom, pdf::PageSpecs, const std::string& transmission);
    // main thread calls to check if a result is ready
    std::optional<RenderResult> get_result();
private:
    void worker_loop();
    void coordinator_loop();
    void dispatch_page_write(const RenderRequest& req);
    void cache_page(int page_num, float zoom,
                    const std::shared_ptr<SharedMemory> &shm,
                    const std::shared_ptr<Tempfile> &tempfile,
                    const std::string &transmission, int page_width, int page_height);

    std::optional<PageCacheData> try_page_cache(const RenderRequest &req, std::shared_ptr<SharedMemory> &shm_ptr,
                        std::shared_ptr<Tempfile> tempfile_ptr);

    std::optional<pdf::DisplayListHandle> fetch_display_list(int page_num);


    // core
    std::unique_ptr<pdf::Parser> parser; // thread local parser
    std::thread worker;
    std::atomic<bool> running = true;
    std::atomic<size_t> current_req_id = 0;

    // threadpool for heavy work
    int n_threads_;
    std::unique_ptr<ThreadPool> thread_pool;

    // thread safety and synchronisation
    std::mutex state_mutex; // shared between cv_worker and actual worker
    std::condition_variable cv_worker;

    // We use a single optional for input to automatically "drop" older frames
    std::optional<RenderRequest> pending_request;

    // variable to keep track of latest result;
    std::optional<RenderResult> latest_result;
    std::shared_ptr<SharedMemory> current_shm;
    std::shared_ptr<Tempfile> current_tempfile;

    // lru_cache to cache heavy display lists
    const int dlist_cache_size = 10;
    const std::chrono::milliseconds dlist_cache_time_limit = std::chrono::milliseconds(100);
    LRUCache<int, pdf::DisplayListHandle> dlist_cache = LRUCache<int, pdf::DisplayListHandle>(dlist_cache_size);

    // lru_cache to cache heavy pages
    const int page_cache_size = 10;
    const std::chrono::milliseconds page_cache_time_limit = std::chrono::milliseconds(100);
    LRUCache<PageDetails, PageCacheData> page_cache = LRUCache<PageDetails, PageCacheData>(page_cache_size);

};

