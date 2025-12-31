#include "render_engine.h"

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#endif
RenderEngine::RenderEngine(const pdf::Parser &prototype_parser, int n_threads,
                           bool use_cache)
    : use_cache(use_cache) {
  // parser created first because during shutdown, any context from parser must
  // be cleared after threadpool shutdown
  parser = prototype_parser.duplicate();
  worker = std::thread(&RenderEngine::coordinator_loop, this);
  n_threads_ = n_threads;
  thread_pool = std::make_unique<ThreadPool>(prototype_parser, n_threads);
}

RenderEngine::~RenderEngine() {
  running = false;
  cv_worker.notify_all();
  if (worker.joinable())
    worker.join(); // join back to main loop
}

void RenderEngine::request_page(int page_num, float zoom, pdf::PageSpecs ps,
                                const std::string &transmission) {
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    ++current_req_id;
    pending_request =
        RenderRequest{page_num, zoom, ps, current_req_id, transmission};
  }
  cv_worker.notify_one(); // wake worker to render page
}

std::optional<RenderResult>
RenderEngine::get_result() { // get the most recently created image
  std::lock_guard<std::mutex> lock(state_mutex);
  return std::move(latest_result);
}

void RenderEngine::coordinator_loop() {
  /* Main job is to wake on new request, then break down and equeue tasks to the
   * threadpool to execute
   */

  while (running) {
    RenderRequest req;
    // wait for work
    {
      std::unique_lock<std::mutex> lock(state_mutex);
      cv_worker.wait(
          lock, [this] { return pending_request.has_value() || !running; });

      if (!running)
        break;

      req = std::move(pending_request.value());
      pending_request.reset();
    }
    dispatch_page_write(req);
  }
}

void RenderEngine::dispatch_page_write(const RenderRequest &req) {
  ZoneScopedN("dispatch_page_write");
  auto start = std::chrono::steady_clock::now();
  RenderResult result;
  result.req_id = req.req_id;
  result.page_num = req.page_num;
  result.transmission = req.transmission;
  std::shared_ptr<SharedMemory> new_shm = nullptr;
  std::shared_ptr<Tempfile> new_temp = nullptr;

  auto update_frame = [&](int page_width, int page_height, int render_time_ms) {
    result.req_id = req.req_id;
    result.zoom = req.zoom;
    result.page_width = page_width;
    result.page_height = page_height;
    result.render_time_ms = render_time_ms;
    std::lock_guard<std::mutex> lock(state_mutex);
    if (new_shm) {
      current_shm = std::move(new_shm);
    }
    if (new_temp) {
      current_tempfile = std::move(new_temp);
    }
    latest_result = std::move(result);
  };

  // check cache for page data first
  auto cached =
      use_cache ? try_page_cache(req, new_shm, new_temp) : std::nullopt;
  if (cached.has_value()) {
    auto data = cached.value();
    result.path_to_data =
        data.transmission == "shm" ? new_shm->name() : new_temp->path();
    update_frame(data.page_width, data.page_height,
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count());
    return;
  }
  // prepare data then enqueue to threadpool
  try {
    auto dlist = fetch_display_list(req.page_num);
    if (!dlist.has_value()) {
      result.error_message = "Failed to generate display list";
      {
        std::lock_guard<std::mutex> lock(state_mutex);
        latest_result = std::move(result);
      }
      return;
    }
    pdf::PageSpecs ps = req.scaled_page_specs;
    auto bounds = parser->split_bounds(ps, n_threads_);
    std::vector<std::future<void>> futures;
    auto start_parse = std::chrono::steady_clock::now();
    void *buffer = nullptr;

    // set up pointers and buffers
    if (req.transmission == "shm") {
      new_shm = std::make_unique<SharedMemory>(ps.size);
      buffer = new_shm->data();
      result.path_to_data = new_shm->name();
    } else {
      new_temp = std::make_unique<Tempfile>(ps.size);
      buffer = new_temp->data();
      result.path_to_data = new_temp->path();
    }

    // enqueue jobs
    for (auto h_bound : bounds) {
      auto fut = thread_pool->enqueue_with_future(
          [h_bound, req, dlist, buffer](pdf::Parser &parser) {
            parser.write_section(req.page_num, h_bound.width, h_bound.height,
                                 req.zoom, 0, dlist.value(),
                                 static_cast<unsigned char *>(buffer) +
                                     h_bound.offset,
                                 h_bound.rect);
          });
      futures.push_back(std::move(fut));
    }
    // wait for future, then update result
    for (auto &fut : futures) {
      fut.get();
    }

    result.transmission = req.transmission;
    result.zoom = req.zoom;
    auto end = std::chrono::steady_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start_parse);
    auto full_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if (use_cache && write_duration > page_cache_time_limit) {
      cache_page(req.page_num, req.zoom, new_shm, new_temp, req.transmission,
                 ps.width, ps.height);
    }
    update_frame(ps.width, ps.height, full_duration.count());
  } catch (const std::exception &e) {
    result.error_message = e.what();
  }
}

std::optional<pdf::DisplayListHandle>
RenderEngine::fetch_display_list(int page_num) {
  ZoneScoped;
  if (use_cache) {
    auto cache_check = dlist_cache.get(page_num);
    if (cache_check.has_value()) { // exists, use cache
      return cache_check.value();
    }
  }
  const auto start = std::chrono::steady_clock::now();
  auto dlist = parser->get_display_list(page_num);
  const auto end = std::chrono::steady_clock::now();

  if (dlist) {
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if (use_cache && duration >= dlist_cache_time_limit) {
      dlist_cache.put(page_num, dlist);
    }
    return dlist;
  }
  return {};
}

void RenderEngine::cache_page(int page_num, float zoom,
                              const std::shared_ptr<SharedMemory> &shm,
                              const std::shared_ptr<Tempfile> &tempfile,
                              const std::string &transmission, int page_width,
                              int page_height) {
  std::vector<unsigned char> buffer;
  if (shm) {
    buffer.resize(shm->size());
    shm->copy_data(buffer.data(), buffer.size());
  }
  page_cache.put({page_num, zoom}, {transmission, std::move(buffer), tempfile,
                                    page_width, page_height});
}

std::optional<PageCacheData>
RenderEngine::try_page_cache(const RenderRequest &req,
                             std::shared_ptr<SharedMemory> &shm_ptr,
                             std::shared_ptr<Tempfile> &tempfile_ptr) {
  auto cached_page = page_cache.get({req.page_num, req.zoom});
  if (!cached_page.has_value())
    return {};

  PageCacheData data = cached_page.value();
  if (data.transmission == "shm") {
    shm_ptr = std::make_unique<SharedMemory>(data.shm_buffer.size());
    shm_ptr->write_data(data.shm_buffer.data(), data.shm_buffer.size());
  } else {
    tempfile_ptr = data.tempfile_data;
  }
  if (!shm_ptr && !tempfile_ptr) { // our cache stores empty data
    throw std::runtime_error("Cache retrival failed");
    // TODO change this to update error message to display on main page  or fall
    // through and rerender update lru_cache to remove this entry since its
    // carrying null data
  }
  return data;
}