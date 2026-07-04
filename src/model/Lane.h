#pragma once

// RollForge — Lane: a row of steps targeting one pad (pure data).
//
// MODEL LAYER RULE: no JUCE GUI includes. Fixed-capacity (maxStepsPerLane) so it
// is trivially copyable into the pattern snapshot; `length` sets how many steps
// are active (allowing per-lane polyrhythms alongside `triplet`).

#include "model/Step.h"

#include <array>

namespace rollforge
{

inline constexpr int maxStepsPerLane = 64;

struct Lane
{
    std::array<Step, maxStepsPerLane> steps {};
    int  length    = 16;      // active steps, 1..maxStepsPerLane
    bool triplet   = false;   // triplet subdivision for this lane
    int  targetPad = 0;       // pad index this lane triggers (0..15)

    Step&       step (int index)       noexcept { return steps[(size_t) index]; }
    const Step& step (int index) const noexcept { return steps[(size_t) index]; }
};

} // namespace rollforge
