#pragma once

// RollForge — RollCompiler: turns a RollRegion into tempo-independent, step-offset
// hit events (a CompiledRoll). Pure and deterministic (no JUCE, no RNG), so it is
// fully headless-tested. Runs on the MESSAGE thread (whoever paints/edits a roll);
// the audio thread only scales step -> sample at play time, never compiling.

#include "model/RollRegion.h"

#include <vector>

namespace rollforge
{
namespace RollCompiler
{
    /** The eased value of a curve at position t in [0,1]. */
    float curveValue (const RollCurve& curve, float t) noexcept;

    /** Fills `out` (capacity `maxEvents`) with the roll's step-offset hits and
        returns the count. Hits are placed by integrating the speed curve (an
        accelerating speed packs them tighter); velocity/pitch follow their curves.
        Allocation-free and deterministic. */
    int compileRollInto (const RollRegion& region, RollEvent* out, int maxEvents) noexcept;

    /** Convenience wrapper returning a vector (message thread / tests). */
    std::vector<RollEvent> compileRoll (const RollRegion& region);

    /** Compiles a region into a CompiledRoll (target pad + start step + events),
        ready to drop into a Pattern's roll list. */
    CompiledRoll compile (const RollRegion& region);
}
} // namespace rollforge
