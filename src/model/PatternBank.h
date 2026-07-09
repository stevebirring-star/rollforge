#pragma once

// RollForge — PatternBank: 8 pattern slots (A..H) with a current + queued slot.
//
// MODEL LAYER RULE: no JUCE GUI includes. Pure data owned by the app (message
// thread). The app hands the current/queued slot's Pattern to the Sequencer
// (immediately, or queued for the next bar); the bar-boundary switch itself is
// the Sequencer's job.

#include "model/Pattern.h"

#include <array>
#include <cstddef>

namespace rollforge
{

inline constexpr int numPatternSlots = 8;   // A..H

struct PatternBank
{
    std::array<Pattern, numPatternSlots> slots {};
    int currentSlot = 0;
    int queuedSlot  = -1;   // -1 = nothing queued

    Pattern&       pattern (int slot)       noexcept { return slots[(std::size_t) slot]; }
    const Pattern& pattern (int slot) const noexcept { return slots[(std::size_t) slot]; }

    static constexpr bool isValidSlot (int slot) noexcept { return slot >= 0 && slot < numPatternSlots; }
};

} // namespace rollforge
