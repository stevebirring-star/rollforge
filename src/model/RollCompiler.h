#pragma once

// RollForge — RollCompiler: turns a RollRegion into concrete, sample-accurate hit
// events. Pure and deterministic (no JUCE, no RNG) so it is fully headless-tested;
// the Sequencer feeds the events into its pending-event scheduler (next commit).

#include "model/RollRegion.h"

#include <vector>

namespace rollforge
{
namespace RollCompiler
{
    /** The eased value of a curve at position t in [0,1]. */
    float curveValue (const RollCurve& curve, float t) noexcept;

    /** Compiles the roll into hit events. `samplesPerStep` is the 1/16-note
        duration in samples (from BPM + sample rate). Hits are placed by
        integrating the speed curve, so an accelerating speed packs them tighter
        over the span; velocity/pitch follow their curves. Returns {} for a
        zero-length roll. */
    std::vector<RollEvent> compileRoll (const RollRegion& region, double samplesPerStep);
}
} // namespace rollforge
