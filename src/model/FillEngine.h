#pragma once

// RollForge — FillEngine: rule-based, per-style drum fills. Given a style,
// intensity (1..5) and a seed, it writes a deterministic fill (steps + a snare
// roll) into a Pattern. Same inputs -> identical output, so it is fully
// headless-tested. Pure (no JUCE).

#include "model/Pattern.h"

#include <cstdint>

namespace rollforge
{
namespace FillEngine
{
    enum Style
    {
        Trap = 0, Drill, House, DnB, BoomBap, Techno, Pop,
        NumStyles
    };

    const char* styleName (Style style) noexcept;

    /** Overwrites `pattern` with a fill for the given style. `intensity` (clamped
        to 1..5) scales the density; `seed` makes the random elements reproducible. */
    void generateFill (Pattern& pattern, Style style, int intensity, std::uint64_t seed);
}
} // namespace rollforge
