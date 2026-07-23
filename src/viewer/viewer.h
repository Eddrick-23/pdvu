#pragma once
#include "pageview.h"
#include "render/parser.h"
#include "render/render_engine.h"
#include "terminal/terminal.h"

class Viewer {
 public:
  Viewer(std::unique_ptr<pdf::Parser> main_parser, std::unique_ptr<RenderEngine> render_engine,
      bool use_shm);
  void run();  // main loop

 private:
  /**
   * @brief Wraps standard 2D pixel or grid dimensions
   */
  struct Dimensions {
    int height;
    int width;
  };

  /**
   * @brief Parameters for redrawing a frame, tracking previous vs. requested size.
   */
  struct FrameDisplayParams {
    Dimensions existing;
    Dimensions target;
  };

  // helper functions
  bool fetch_latest_frame();
  void display_latest_frame(const FrameDisplayParams& params);
  void request_page_render(int page_num);
  void handle_page_pan(char key);
  void handle_go_to_page();
  void handle_help_page();
  void process_keypress();
  [[nodiscard]] Dimensions available_window();
  // sub systems
  Terminal m_term;                           // terminal data and raw mode
  std::unique_ptr<pdf::Parser> m_parser;     // parsing pdfs
  std::unique_ptr<RenderEngine> m_renderer;  // loading page frames
  PageView m_page_view;                      // zoom and panning handling

  // current state
  int m_current_page = 0;
  int m_total_pages = 0;
  int m_rotation_degrees = 0;
  bool m_running = false;
  bool m_shm_supported = false;
  size_t m_last_req_id = 0;
  pdf::PageSpecs m_target_page_specs = {};
  RenderResult m_latest_frame = RenderResult{};
};