#pragma once

// RollForge — GridGeometry: how a row of N cells divides a width, exactly.
//
// The obvious `cellW = width / count` throws away the remainder, so a 16-column grid in a
// 649 px space leaves 9 px of dead air on the right — and the amount changes as the window
// resizes, which is what makes a layout look like it drifts. The fix is to compute each
// cell's EDGE by rounding, never its width by dividing: edge(i) = round(i * width / count).
// Consecutive edges then tile the space with no gap and no overlap, at any size.
//
// It lives in its own header because the sequencer grid and the roll-brush overlay that
// sits on top of it must agree to the pixel. Two copies of the same formula is one copy
// too many; the overlay drifting off the grid is exactly the bug this prevents.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

/** The pixel edge of cell `index` in a run of `count` cells spanning `total` pixels. */
inline int gridEdge (int index, int count, int total) noexcept
{
    if (count <= 0)
        return 0;
    return juce::roundToInt ((double) index * (double) total / (double) count);
}

/** The bounds of cell `index` within `[origin, origin + total)`. */
inline juce::Range<int> gridSpan (int index, int count, int total, int origin = 0) noexcept
{
    const int a = origin + gridEdge (index, count, total);
    const int b = origin + gridEdge (index + 1, count, total);
    return { a, juce::jmax (a + 1, b) };
}

/** Which cell contains `position`, clamped into [0, count). The inverse of gridSpan. */
inline int gridIndexAt (int position, int count, int total, int origin = 0) noexcept
{
    if (count <= 0 || total <= 0)
        return 0;
    const double f = (double) (position - origin) / (double) total;
    return juce::jlimit (0, count - 1, (int) (f * (double) count));
}

/** The bounds of cell (col, row) of a cols x rows grid filling `area`. */
inline juce::Rectangle<int> gridCell (int col, int row, int cols, int rows,
                                      juce::Rectangle<int> area) noexcept
{
    const auto x = gridSpan (col, cols, area.getWidth(),  area.getX());
    const auto y = gridSpan (row, rows, area.getHeight(), area.getY());
    return { x.getStart(), y.getStart(), x.getLength(), y.getLength() };
}

} // namespace rollforge
