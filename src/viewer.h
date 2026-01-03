#pragma once
#include "parser.h"
#include "render_engine.h"
#include "terminal.h"

class Viewer {
public:
  Viewer(std::unique_ptr<pdf::Parser> main_parser,
         std::unique_ptr<RenderEngine> render_engine, bool use_shm);
  void run(); // main loop

private:
  struct CropRect { // (0,0) is taken to be top left corner
    int x_offset_pixels;
    int y_offset_pixels;
    int width;
    int height;
  };
  struct Viewport {
    float rel_x_offset;
    float rel_y_offset;
    int zoom_index;
    float zoom;
  };
  // helper functions
  bool fetch_latest_frame();
  void display_latest_frame();
  void request_page_render(int page_num);
  void change_zoom_index(int delta);
  CropRect calculate_crop_window();
  void update_viewport(float delta_x, float delta_y);
  void handle_page_pan(char key);
  void handle_go_to_page();
  void handle_help_page();
  void process_keypress();
  // sub systems
  Terminal term;                          // terminal data and raw mode
  std::unique_ptr<pdf::Parser> parser;    // parsing pdfs
  std::unique_ptr<RenderEngine> renderer; // loading page frames

  // current state
  int current_page = 0;
  int total_pages = 0;
  bool running = false;
  bool shm_supported = false;
  size_t last_req_id = 0;
  RenderResult latest_frame = RenderResult{};

  // control zoom and panning
  static constexpr std::array<float, 11> zoom_levels{
      0.5, 0.67, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0};
  static constexpr int default_zoom_index = 5; // 1.0x (100%)
  Viewport viewport{0.0, 0.0, default_zoom_index,
                    zoom_levels[default_zoom_index]};
};