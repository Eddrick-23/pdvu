#pragma once
#include <cstddef>

#include "pageview.h"
#include "render/parser.h"
#include "render/render_engine.h"
#include "terminal/inputbar.h"
#include "terminal/terminal.h"
#include "terminal/tui.h"
#include "utils/geometry.h"
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
   */
  void run();  // main loop

 private:
  /**
   * @brief Wraps standard two-dimensional pixel or grid dimensions.
   */
  struct Dimensions {
    int width;
    int height;
  };

  /**
   * @brief Dimensions used to place the currently available frame.
   *
   * existing describes the source bitmap in RenderState::latest_frame. target
   * describes the desired placement in RenderState::target_state. The dimensions
   * may differ while a compatible same-page render is pending, allowing the
   * existing bitmap to provide an immediate scaled preview.
   */
  struct FrameDisplayParams {
    Dimensions existing;
    Dimensions target;
  };

  /**
   * @brief defines frame layout to pass to kitty for image rendering
   *
   * @note We hold two crop rectangles here and for a specific reason.
   * Kitty can perform image "upscaling" by taking an existing bitmap and specifying dimesions
   * larger than it. E.g. 200 * 200 native, but we specify 400 * 400 when rendering.
   * To support responsive zooming, we can use the current rendered image and "upscale" to
   * the target dimensions. However, if the target image needs to be cropped, we first calculate
   * the crop window if our image is indeed the target dimensions. Then scale that crop window
   * up/down to fit the source dimensions. Kitty will then apply the source crop window to our
   * current
   * source bitmap then upscale that to fill our screen, achieving the responsive zoom effect while
   * the native frame is rendering.
   */
  struct FrameLayout {
    geometry::PixelRect target_crop_rect;     ///< crop rectangle based on target dimensions
    geometry::PixelRect source_crop_rect;     ///< crop rectangle based on current frame dimensions
    geometry::CellPosition placement_origin;  ///< top left terminal cell to start drawing image
    int placement_cols;                       ///< number of terminal cell cols image will take
    int placement_rows;                       ///< number of terminal cell rows image will take
    bool is_frame_native;                     ///< Whether existing bitmap matches target dimensions
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
   * Results whose request ID does not match the current RenderTarget are discarded
   * as superseded. A matching successful result replaces RenderState::latest_frame.
   * Matching render errors are displayed but are not installed. The function does
   * not decide whether the active UI mode should be redrawn.
   *
   * @return true when a new successful frame was stored; otherwise false.
   */
  bool fetch_latest_frame();

  /**
   * @brief Builds the terminal sequence that places the latest page image.
   *
   * Calculates source cropping and target placement from the displayed bitmap
   * dimensions, desired target dimensions, terminal layout, and PageView offsets.
   * When the dimensions differ, the target crop is mapped back into source-bitmap
   * coordinates so Kitty can scale the existing image as a preview.
   *
   * @pre The caller has verified that the displayed frame and render target are
   * preview-compatible.
   *
   * @param params Existing bitmap dimensions and currently requested dimensions.
   * @return ANSI and Kitty protocol sequence for the page image.
   */
  std::string latest_frame_sequence(const FrameDisplayParams& params);

  /**
   * @brief Draws the latest page frame with the requested status bars.
   *
   * Displays the minimum-size guard instead when the terminal is too small.
   * While a newer compatible render is pending, draws the existing bitmap scaled
   * to the target geometry for immediate visual feedback. If the pending target
   * refers to another page or rotation, preserves the existing presentation until
   * the target frame arrives. Otherwise, composes the Kitty page placement with
   * the selected bars, writes the complete sequence to stdout, and flushes it.
   *
   * @param with_top_bar Whether to include the top status bar.
   * @param with_bottom_bar Whether to include the bottom status bar.
   */
  void draw_latest_frame(bool with_top_bar, bool with_bottom_bar);

  /**
   * @brief Requests an asynchronous render for the desired page state.
   *
   * Reads the current terminal dimensions, page rotation, and zoom state;
   * calculates the scaled target PageSpecs; dispatches a non-blocking request to
   * the render engine; and stores the returned generation ID, page number, and
   * specs together in RenderState::target_state. Does nothing if the terminal is
   * too small or the page number is invalid.
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
   * @brief Handles one input event while GoToPage is active.
   *
   * Updates a m_go_to_page which holds a TUI::InputBar component.
   * The component tracks buffer and cursor state.
   * When the minimum-size guard is visible, only q is accepted and
   * requests application shutdown.
   *
   * @param event Decoded terminal input event.
   * @return true when the current mode should be redrawn immediately.
   */
  bool handle_go_to_page_input(const InputEvent& event);

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
   * immediately. The preview-compatibility guard also prevents incidental redraws
   * from combining the old bitmap with the rotated target geometry; the completed
   * render triggers the redraw instead.
   */
  bool handle_browse_input(const InputEvent& event);

  /**
   * @brief Draws the complete presentation for the active UI mode.
   *
   * - Browse draws the page and both status bars.
   * - GoToPage draws the page and top bar while reserving the bottom row for its
   * input component.
   * - Help clears and redraws its overlay using the current terminal dimensions.
   */
  void draw_for_current_mode();

  /**
   * @brief Reads and routes one terminal input event.
   *
   * Waits up to INPUT_POLL_RATE_MS for an event, then forwards it to the input handler
   * for the active mode.
   *
   * @return true when the active mode should be redrawn immediately.
   */
  bool process_keypress();

  /**
   * @brief Calculates the usable pixel dimensions of the terminal window, reserving two rows for
   * the top and bottom status bars.
   */
  [[nodiscard]] Dimensions available_window();

  /**
   * @brief Checks whether the displayed bitmap may be scaled as a preview of the target.
   *
   * Preview scaling is safe when both states refer to the same page and rotation.
   * Zoom level and pixel dimensions are intentionally not compared because those
   * differences are what the preview is designed to bridge.
   *
   * @return true when the existing bitmap can represent the pending target through
   * scaling and cropping; otherwise false.
   */
  [[nodiscard]] bool is_preview_compatible() const;

  /**
   * @breif calculates the target frame layout given source, target dimensions and the target
   * crop window.
   *
   * Internally, target_crop is scaled to a new crop window to match source dimensions (if
   * dimensions differ)
   * @param source current bitmap dimensions
   * @param target target bitmap dimensions
   * @param target_crop crop window for target bitmap's image
   * @param ts current terminal size
   * @param content_area drawable content area
   * @return
   */
  [[nodiscard]] FrameLayout calculate_frame_layout(geometry::PixelSize source,
                                                   geometry::PixelSize target,
                                                   geometry::PixelRect target_crop,
                                                   const TermSize& ts,
                                                   const TUI::ContentArea& content_area) const;

  // subsystems
  Terminal m_term;                           // terminal data and raw mode
  std::unique_ptr<pdf::Parser> m_parser;     // parsing pdfs
  std::unique_ptr<RenderEngine> m_renderer;  // loading page frames
  PageView m_page_view;                      // zoom and panning handling

  /**
   * @brief Identifies the active UI mode and determines input routing and drawing.
   */
  enum class UiMode {
    Browse,    ///< Normal document controls
    GoToPage,  ///< Page-number entry through TUI::InputBar
    Help,      ///< Viewing help ui page
  };

  /**
   * @brief Describes the most recently requested render generation.
   *
   * The fields are updated together after a render request is dispatched. They
   * describe desired state and may differ from the bitmap in RenderState::latest_frame
   * until the matching asynchronous result is accepted.
   */
  struct RenderTarget {
    std::size_t req_id = 0;       ///< Generation ID returned for the request
    int page_num = 0;             ///< Zero-based page requested
    pdf::PageSpecs page_specs{};  ///< Scaled and rotated geometry requested
  };

  /**
   * @brief Tracks desired render state and the most recently accepted bitmap.
   *
   * target_state represents what the viewer wants next. latest_frame represents
   * the bitmap currently available to display. Their request IDs differ while a
   * render is pending. last_transmitted_req_id records which accepted bitmap has
   * already been sent to the terminal so redraws can reuse its Kitty image ID.
   */
  struct RenderState {
    RenderTarget target_state{};
    std::size_t last_transmitted_req_id =
        0;  ///< Render generation most recently transmitted to terminal
    RenderResult latest_frame =
        RenderResult{};  ///< Most recently accepted successful render available for display.
  };

  struct GoToPageState {
    TUI::InputBar input{"GO TO PAGE: "};
    void reset() { input.reset(); }
  };

  // current state
  UiMode m_ui_mode = UiMode::Browse;
  int m_current_page = 0;           ///< Desired zero-based page number
  int m_total_pages = 0;            ///< Number of pages in loaded document
  int m_rotation_degrees = 0;       ///< Desired clockwise rotation
  bool m_running = false;           ///< Controls main application loop
  GoToPageState m_go_to_page = {};  ///< Track Go To Page Ui state

  // configuration
  bool m_shm_supported = false;  ///< Whether shared-memory transmission is enabled

  // Rendering state
  RenderState m_render;
};
