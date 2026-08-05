#pragma once
#include "pageview.h"
#include "render/parser.h"
#include "render/render_engine.h"
#include "terminal/terminal.h"
#include "utils/resize_debouncer.h"

class Viewer {
 public:
  /**
   * @brief Initializes the Viewer, setting up the parser, render engine, and checking shared memory
   * support.
   */
  Viewer(std::unique_ptr<pdf::Parser> main_parser, std::unique_ptr<RenderEngine> render_engine,
         bool use_shm);

  /**
   * @brief Runs the application event and rendering loop.
   *
   * Initializes the terminal session, polls and routes input according to the
   * active UI mode, handles debounced terminal resizing, collects asynchronous
   * page-render results, and redraws the active mode when invalidated.
   *
   * @Note GoToPage currently remains a blocking modal input loop; Browse and Help are
   * handled directly by this main loop.
   */
  void run();  // main loop

 private:
  /**
   * @brief Identifies the active UI mode and determines input routing and drawing.
   */
  enum class UiMode {
    Browse,    ///< Normal document controls
    GoToPage,  ///< Currently delegates input blocking to TUI::bottom_input_bar
    Help,      ///< Viewing help ui page
  };

  /**
   * @brief Wraps standard 2D pixel or grid dimensions
   */
  struct Dimensions {
    int width;
    int height;
  };

  /**
   * @brief Dimensions used to place the currently available frame.
   *
   * existing describes the rendered bitmap in m_latest_frame. target describes
   * the dimensions currently requested by the viewer and may differ while a new
   * render is pending.
   */
  struct FrameDisplayParams {
    Dimensions existing;
    Dimensions target;
  };

  /**
   * @brief Advances the shared resize debouncer and applies resize policy.
   *
   * A resize in progress invalidates the active UI so it can be redrawn using
   * the latest terminal dimensions. Once resizing settles, a new page render is
   * requested at the final dimensions, including while Help is active.
   *
   * @param debouncer Resize state (injected by the main event loop).
   * @return true when the active UI should be redrawn; otherwise false.
   */
  bool handle_resize(ResizeDebouncer& debouncer);

  /**
   * @brief Polls the render engine for a newly completed page frame.
   *
   * Stores a successful result in m_latest_frame. Duplicate results are ignored,
   * while render errors are displayed and are not installed as the latest frame.
   * The function does not decide whether the active UI mode should be redrawn.
   *
   * @return true when a new successful frame was stored; otherwise false.
   */
  bool fetch_latest_frame();

  /**
   * @brief Builds the terminal sequence that places the latest page image.
   *
   * Calculates cropping and placement from the currently rendered dimensions,
   * requested target dimensions, terminal layout, and PageView offsets.
   *
   * @param params Existing bitmap dimensions and currently requested dimensions.
   * @return ANSI and Kitty protocol sequence for the page image.
   */
  std::string latest_frame_sequence(const FrameDisplayParams& params);

  /**
   * @brief Draws the latest page frame with the requested status bars.
   *
   * Displays the minimum-size guard instead when the terminal is too small.
   * Otherwise, composes the Kitty page placement with the selected bars, writes
   * the complete sequence to stdout, and flushes it.
   *
   * @param with_top_bar Whether to include the top status bar.
   * @param with_bottom_bar Whether to include the bottom status bar.
   */
  void draw_latest_frame(bool with_top_bar, bool with_bottom_bar);

  /**
   * @brief Requests an asynchronous render for the desired page state.
   *
   * Reads the current terminal dimensions, page rotation, and zoom state;
   * calculates and stores m_target_page_specs; and dispatches a non-blocking
   * request to the render engine. Does nothing if the terminal is too small or
   * the page number is invalid.
   *
   * @param page_num Zero-based page number to render.
   */
  void request_page_render(int page_num);

  /**
   * @brief Applies a directional pan command to PageView.
   *
   * @param key One of w, a, s, d or its uppercase equivalent(move by larger step).
   * @return true when the viewport offset changed and Browse should be redrawn.
   */
  bool handle_page_pan(char key);

  /**
   * @brief Runs the blocking go-to-page input mode.
   *
   * Delegates editing and terminal input to TUI::bottom_input_bar(), validates
   * and bounds the submitted one-based page number, updates m_current_page, and
   * requests a render when the page changes.
   *
   * While active, injected callbacks keep completed frames and resize behavior
   * responsive. The caller owns transitions into and out of UiMode::GoToPage
   * and schedules the final Browse redraw.
   *
   * @note This is currently the only UI handler that owns a nested input loop.
   */
  void handle_go_to_page();

  /**
   * @brief Handles one input event while Help is active.
   *
   * Pressing q requests application shutdown. Escape returns to Browse when the
   * terminal is large enough and clears the Kitty dim layer. Other events do not
   * modify state.
   *
   * @param event Decoded terminal input event.
   * @return true when the event causes a Help exit or shutdown transition.
   */
  bool handle_help_input(const InputEvent& event);

  /**
   * @brief Handles one input event while Browse is active.
   *
   * Applies document navigation, zoom, rotation, and pan commands, or transitions
   * to Help and GoToPage. While the minimum-size guard is visible, only q is
   * accepted and requests application shutdown.
   *
   * @param event Decoded terminal input event.
   * @return true when the current mode should be redrawn immediately.
   *
   * @note Rotation intentionally returns false so the old frame is not placed
   * using the newly rotated target geometry; the completed render triggers the
   * redraw instead.
   */
  bool handle_browse_input(const InputEvent& event);

  /**
   * @brief Draws the complete presentation for the active UI mode.
   * - Browse draws the page and both status bars.
   * - GoToPage draws the page and top bar while reserving the bottom row for its input component.
   * - Help clears and redraws its overlay using the current terminal dimensions.
   */
  void draw_for_current_mode();

  /**
   * @brief Reads and routes one terminal input event.
   *
   * Waits up to INPUT_POLL_RATE_MS for an event, then forwards it to the input handler
   * for the active mode. GoToPage input is currently consumed by its blocking TUI
   * loop rather than through this function.
   *
   * @return true when the active mode should be redrawn immediately.
   */
  bool process_keypress();

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
  UiMode m_ui_mode = UiMode::Browse;

  // current state
  int m_current_page = 0;        ///< Desired zero-based page number
  int m_total_pages = 0;         ///< Number of pages in loaded document
  int m_rotation_degrees = 0;    ///< Desired clockwise rotation
  bool m_running = false;        ///< Controls main application loop
  bool m_shm_supported = false;  ///< Whether shared-memory transmission is enabled
  size_t m_last_req_id = 0;      ///< Render generation most recently transmitted to terminal
  pdf::PageSpecs m_target_page_specs = {};
  // Scaled and rotated dimensions currently requested from the render engine.
  // These may differ from m_latest_frame while a render is pending.

  RenderResult m_latest_frame = RenderResult{};
  // Most recently accepted successful render available for display.
};