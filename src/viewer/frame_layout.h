#pragma once
#include "terminal/terminal.h"
#include "terminal/tui.h"
#include "utils/geometry.h"
namespace viewer {
/**
 * @brief defines frame layout to pass to kitty for image rendering
 *
 * @note We hold two crop rectangles here and for a specific reason.
 * Kitty can perform image "upscaling" by taking an existing bitmap and specifying dimensions
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
  bool source_matches_target;  ///< The existing bitmap dimensions matches the target dimensions
};

/**
 * @brief calculates the target frame layout given source, target dimensions and the target
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
FrameLayout calculate_frame_layout(geometry::PixelSize source, geometry::PixelSize target,
                                   geometry::PixelRect target_crop, const TermSize& ts,
                                   const TUI::ContentArea& content_area);
}  // namespace viewer