#pragma once

// RollForge — Kit: a 4x4 grid of 16 Pads plus kit metadata.
//
// MODEL LAYER RULE: no JUCE GUI includes. Pure data, unit-testable. A Kit is
// the message-thread model that a project saves/loads; copying a Kit deep-copies
// its pads (incrementing the shared samples' reference counts), which is what
// makes it usable as an undo snapshot in a later phase.

#include "model/Pad.h"

namespace rollforge
{

/** The pad grid is 4 columns x 4 rows. */
inline constexpr int kitNumColumns = 4;
inline constexpr int kitNumRows    = 4;
inline constexpr int kitNumPads    = kitNumColumns * kitNumRows;   // 16

struct Kit
{
    juce::String              name { "Empty Kit" };
    std::array<Pad, kitNumPads> pads {};

    //==========================================================================
    /** Linear pad index for a (column, row) coordinate; row 0 is the top row. */
    static constexpr int indexFor (int column, int row) noexcept
    {
        return row * kitNumColumns + column;
    }

    static constexpr bool isValidIndex (int index) noexcept
    {
        return index >= 0 && index < kitNumPads;
    }

    //==========================================================================
    /** Pad by linear index 0..15. */
    Pad&       pad (int index)       noexcept { jassert (isValidIndex (index)); return pads[(size_t) index]; }
    const Pad& pad (int index) const noexcept { jassert (isValidIndex (index)); return pads[(size_t) index]; }

    /** Pad by (column, row). */
    Pad&       pad (int column, int row)       noexcept { return pad (indexFor (column, row)); }
    const Pad& pad (int column, int row) const noexcept { return pad (indexFor (column, row)); }

    /** True if any pad has a sample assigned. */
    bool hasAnySample() const noexcept
    {
        for (auto& p : pads)
            if (p.hasSample())
                return true;
        return false;
    }
};

} // namespace rollforge
