#include "render_engine.h"

#include <cstddef>

#include "bounds.h"
#include "plog/Log.h"
#include "utils/logging.h"
#include "utils/profiling.h"

RenderEngine::RenderEngine(const pdf::Parser& prototype_parser, int n_threads, bool use_cache)
    : n_threads_(n_threads), use_cache(use_cache) {
  // parser created first because during shutdown, any context from parser must
  // be cleared after threadpool shutdown
  parser = prototype_parser.duplicate();
  for (auto i = 0; i < n_threads; i++) {
    worker_parsers.emplace_back(prototype_parser.duplicate());
  }

  thread_pool = std::make_unique<ThreadPool>(static_cast<std::size_t>(n_threads));
  worker = std::thread(&RenderEngine::coordinator_loop, this);
}

RenderEngine::~RenderEngine() {
  running = false;
  cv_worker.notify_all();
  if (worker.joinable()) worker.join();  // join back to main loop
}

std::size_t RenderEngine::request_page(int page_num, float zoom, pdf::PageSpecs ps,
                                       const std::string& transmission) {
  std::size_t id;
  {
    std::scoped_lock lock(state_mutex);
    id = ++current_req_id;
    pending_request = RenderRequest{
        .page_num = page_num,
        .zoom = zoom,
        .scaled_page_specs = ps,
        .req_id = id,
        .transmission = transmission,
    };
  }
  cv_worker.notify_one();  // wake worker to render page
  return id;
}

std::optional<RenderResult> RenderEngine::get_result() {  // get the most recently created image
  // want to leave latest_result as a std::nullopt after move
  // std::swap does this for us automatically
  std::scoped_lock lock(state_mutex);
  std::optional<RenderResult> out;
  std::swap(out, latest_result);
  return out;
}

void RenderEngine::coordinator_loop() {
  /* Main job is to wake on new request, then break down and enqueue tasks to the
   * threadpool to execute
   */

  while (running) {
    RenderRequest req;
    // wait for work
    {
      std::unique_lock<std::mutex> lock(state_mutex);
      cv_worker.wait(lock, [this] { return pending_request.has_value() || !running; });

      if (!running) break;

      req = std::move(pending_request.value());
      pending_request.reset();
    }
    dispatch_page_write(req);
  }
}

void RenderEngine::dispatch_page_write(const RenderRequest& req) {
  ZoneScopedN("dispatch_page_write");
  using namespace std::chrono;
  auto start = steady_clock::now();
  RenderResult result{};
  result.req_id = req.req_id;
  result.page_num = req.page_num;
  result.transmission = req.transmission;
  result.rendered_page_specs = req.scaled_page_specs;
  std::shared_ptr<SharedMemory> new_shm = nullptr;
  std::shared_ptr<Tempfile> new_temp = nullptr;

  auto update_frame = [&](int render_time_ms) {
    result.render_time_ms = render_time_ms;
    std::scoped_lock lock(state_mutex);
    if (new_shm) {
      current_shm = std::move(new_shm);
    }
    if (new_temp) {
      current_tempfile = std::move(new_temp);
    }
    latest_result = std::move(result);
  };

  // check cache for page data first
  auto cached = use_cache ? try_page_cache(req, new_shm, new_temp) : std::nullopt;
  if (cached.has_value()) {
    const auto& data = cached.value();
    result.rendered_page_specs = data.rendered_page_specs;
    result.path_to_data = data.transmission == "shm" ? new_shm->name() : new_temp->path();
    result.transmission = data.transmission;
    int duration_ms =
        static_cast<int>(duration_cast<milliseconds>(steady_clock::now() - start).count());
    update_frame(duration_ms);
    return;
  }
  // prepare data then enqueue to threadpool
  try {
    auto dlist = fetch_display_list(req.page_num);
    if (!dlist.has_value()) {
      result.error_message = "Failed to generate display list";
      {
        std::scoped_lock lock(state_mutex);
        latest_result = std::move(result);
      }
      return;
    }
    pdf::PageSpecs ps = req.scaled_page_specs;
    auto bounds = pdf::split_bounds(ps, n_threads_);
    std::vector<std::future<void>> futures;
    auto start_parse = steady_clock::now();
    void* buffer = nullptr;

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
    // since the engine design is that we only ever render one page at once
    // we can use batch-index borrowing where each h_bound uses a parser
    // at a specific index.
    // This is also because we maintain that n_bounds <= n_parsers
    for (std::size_t idx = 0; idx < bounds.size(); idx++) {
      auto h_bound = bounds[idx];
      auto fut = thread_pool->submit([h_bound, req, dlist, buffer, idx, this]() {
        worker_parsers[idx]->write_section(h_bound.width,
                                           h_bound.height,
                                           req.zoom,
                                           req.scaled_page_specs,
                                           dlist.value(),
                                           static_cast<unsigned char*>(buffer) + h_bound.offset,
                                           h_bound.rect);
      });
      futures.push_back(std::move(fut));
    }

    // wait for future, then update result
    // if an exception occurs, capture it and let all other futures drain
    // this ensures we don't close the buffer while other threads
    // are still writing to it.
    std::exception_ptr first_error;
    for (auto& fut : futures) {
      try {
        fut.get();
      } catch (...) {
        if (!first_error) {
          first_error = std::current_exception();
        }
      }
    }

    if (first_error) {
      std::rethrow_exception(first_error);
    }

    result.transmission = req.transmission;
    auto end = steady_clock::now();
    auto write_duration = duration_cast<milliseconds>(end - start_parse);
    auto full_duration = duration_cast<milliseconds>(end - start);
    if (use_cache && write_duration > page_cache_time_limit) {
      cache_page(req, result, new_shm, new_temp);
    }
    update_frame(static_cast<int>(full_duration.count()));
  } catch (const std::exception& e) {
    result.error_message = e.what();
    update_frame(0);
  }
}

std::optional<pdf::DisplayListHandle> RenderEngine::fetch_display_list(int page_num) {
  ZoneScoped;
  if (use_cache) {
    auto cache_check = dlist_cache.get(page_num);
    if (cache_check.has_value()) {  // exists, use cache
      return cache_check;
    }
  }
  const auto start = std::chrono::steady_clock::now();
  auto dlist = parser->get_display_list(page_num);
  const auto end = std::chrono::steady_clock::now();

  if (dlist.has_value()) {
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if (use_cache && duration >= dlist_cache_time_limit) {
      dlist_cache.put(page_num, dlist.value());
    }
    return dlist;
  }
  return {};
}

void RenderEngine::cache_page(const RenderRequest& req, const RenderResult& res,
                              const std::shared_ptr<SharedMemory>& shm,
                              const std::shared_ptr<Tempfile>& tempfile) {
  page_cache.put(
      {
          .page_num = req.page_num,
          .zoom = req.zoom,
          .rotation_degrees = req.scaled_page_specs.rotation,
      },
      {
          .transmission = req.transmission,
          .shm_data = shm,
          .tempfile_data = tempfile,
          .rendered_page_specs = res.rendered_page_specs,
      });
}

std::optional<PageCacheData> RenderEngine::try_page_cache(const RenderRequest& req,
                                                          std::shared_ptr<SharedMemory>& shm_ptr,
                                                          std::shared_ptr<Tempfile>& tempfile_ptr) {
  const auto key = PageDetails{
      .page_num = req.page_num,
      .zoom = req.zoom,
      .rotation_degrees = req.scaled_page_specs.rotation,
  };
  const auto cached_page = page_cache.get(key);
  if (!cached_page.has_value()) return {};

  PageCacheData data = cached_page.value();

  // shm does not allow reusing to we make a copy and write to the buffer
  // tempfile allows reusing so we reuse the same tempfile pointer
  if (data.transmission == "shm") {
    try {
      const size_t segment_size = data.shm_data->size();
      shm_ptr = std::make_unique<SharedMemory>(segment_size);
      const auto status = shm_ptr->write_data(data.shm_data->data(), segment_size);
      if (status != SharedMemory::WriteStatus::Success) {
        PLOG_ERROR << "Render error: failed to write page " << key.page_num
                   << " to new shm buffer for transmission. Reason: "
                   << SharedMemory::to_string(status);
        shm_ptr.reset();
      }
    } catch (const std::exception& e) {
      PLOG_ERROR << "Failed to allocate shm for cached page " << key.page_num << ": " << e.what();
      shm_ptr.reset();
    }
  } else {
    tempfile_ptr = data.tempfile_data;
  }

  // If key exists but there is no data there, wipe entry from cache
  // and rerender
  if (!shm_ptr && !tempfile_ptr) {
    PLOG_INFO << "Cache retrieval failed, key has empty entry";
    page_cache.erase(key);
    return {};
  }

  return data;
}