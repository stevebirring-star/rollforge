#pragma once

// RollForge — Pattern: a set of lanes plus tempo/swing (pure data).
//
// MODEL LAYER RULE: no JUCE GUI includes. Fixed-capacity (maxLanes) and fully
// value-copyable, so a whole Pattern can be published to the audio thread as an
// immutable snapshot via engine/TripleBuffer.h (the audio thread reads it, never
// mutates it, and never blocks).

#include "model/Lane.h"

#include <array>

namespace rollforge
{

inline constexpr int maxLanes = 16;

struct Pattern
{
    std::array<Lane, maxLanes> lanes {};
    int    numLanes = 0;         // active lanes, 0..maxLanes
    double bpm      = 120.0;
    float  swing    = 0.0f;      // 0..1 (delay applied to off-beat 8ths)

    Lane&       lane (int index)       noexcept { return lanes[(size_t) index]; }
    const Lane& lane (int index) const noexcept { return lanes[(size_t) index]; }
};

} // namespace rollforge
