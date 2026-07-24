#pragma once
#include "pageview.h"
#include "render/parser.h"
#include "render/render_engine.h"
#include "terminal/terminal.h"

class Viewer {
 public:
  /**
   * @brief Initializes the Viewer, setting up the parser, render engine, and checking shared memory
   * support.
   */
  Viewer(std::unique_ptr<pdf::Parser> main_parser, std::unique_ptr<RenderEngine> render_engine,
      bool use_shm);

  /**
   * @brief Starts the primary application loop, puts the terminal into raw mode. This handles
   * input, resizing, and rendering until a quit is requested.
   */
  void run();  // main loop

 private:
  /**
   * @brief Wraps standard 2D pixel or grid dimensions
   */
  struct Dimensions {
    int width;
    int height;
  };

  /**
   * @brief Parameters for redrawing a frame, tracking previous vs. requested size.
   */
  struct FrameDisplayParams {
    Dimensions existing;
    Dimensions target;
  };

  // helper functions
  /**
   * @brief Retrieves the most recent rendered frame from the background engine, returning true if a
   * new frame is successfully fetched.
   */
  bool fetch_latest_frame();

  /**
   * @brief Calculates viewport cropping and outputs the terminal graphics sequence (Kitty protocol)
   * to display the frame and top and bottom status bars.
   */
  void display_latest_frame(const FrameDisplayParams& params);

  /**
   * @brief Calculates target page specifications and zoom factor, dispatching a non-blocking render
   * request to the render engine.
   */
  void request_page_render(int page_num);

  /**
   * @brief Translates directional keypresses (w, a, s, d) into viewport offsets and updates the
   * display if the view bounds change.
   */
  void handle_page_pan(char key);

  /**
   * @brief Prompts the user with an interactive input bar to jump to a specific page number,
   * handling input parsing and triggering the page change.
   */
  void handle_go_to_page();

  /**
   * @brief Displays the help overlay and traps input in a sub-loop until dismissed, handling
   * resizes and cleanly restoring the previous view if no rerender occurs.
   */
  void handle_help_page();

  /**
   * @brief Reads raw terminal input and dispatches corresponding actions such as zooming, panning,
   * rotation, navigation, and quitting.
   */
  void process_keypress();

  /**
   * @brief Calculates the usable pixel dimensions of the terminal window, reserving two rows for
   * the top and bottom status bars.
   */
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