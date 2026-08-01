#pragma once
#include <vector>

#include "page_specs.h"

namespace pdf {
/**
 * @brief Partitions a page into n horizontal strips for parallel rendering.
 *
 * Note for invalid inputs:
 * - n < 1 values will be clamped to n = 1
 * - Negative heights in page specs will be clamped to height 0 and
 * return a single empty strip
 *
 * Boundary Coordinates:
 * The calculated regions use half-open intervals [start, end). The lower bounds
 * (x0, y0) are inclusive, and the upper bounds (x1, y1) are exclusive. This guarantees
 * that adjacent strips do not overlap and that their combined heights exactly match
 * the original page height without off-by-one errors.
 *
 * Memory Layout:
 * The `offset` field provides the exact byte starting position for each strip. This allows
 * parallel threads to write their rendered chunks directly into a single, contiguous
 * shared memory cache safely, preparing the buffer for a single-pass transfer to the
 * terminal window.
 * @param ps The PageSpecs of the page to split.
 * @param n The number of horizontal strips to generate.
 * @return A vector of HorizontalBound definitions.
 */
[[nodiscard]] std::vector<HorizontalBound> split_bounds(PageSpecs ps, int n);
}  // namespace pdf