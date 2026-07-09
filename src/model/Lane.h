#pragma once

// RollForge — Lane: a row of steps targeting one pad (pure data).
//
// MODEL LAYER RULE: no JUCE GUI includes. Fixed-capacity (maxStepsPerLane) so it
// is trivially copyable into the pattern snapshot; `length` sets how many steps
// are active (allowing per-lane polyrhythms alongside `triplet`).

#include "model/Step.h"

#include <array>
#include <cstdint>

namespace rollforge
{

inline constexpr int maxStepsPerLane = 64;

inline constexpr int straightStepsPerBar = 16;   // 1/16 notes
inline constexpr int tripletStepsPerBar  = 12;   // 1/8-note triplets: three per quarter

struct Lane
{
    std::array<Step, maxStepsPerLane> steps {};
    int  length    = 16;      // active steps, 1..maxStepsPerLane
    bool triplet   = false;   // this lane's steps are 1/8-note triplets, not 1/16ths
    int  targetPad = 0;       // pad index this lane triggers (0..15)

    Step&       step (int index)       noexcept { return steps[(std::size_t) index]; }
    const Step& step (int index) const noexcept { return steps[(std::size_t) index]; }
};

/** How long one of the lane's steps is, as a rational multiple of a 1/16 step.

    Straight is 1/1. A triplet lane's step is 4/3 of a 1/16 — three of them fill a
    quarter note, twelve fill a bar — so a triplet lane realigns with the bar exactly,
    and PatternBank's bar-boundary switching and the roll grid stay correct. */
struct LaneStepRate { int num = 1; int den = 1; };

inline LaneStepRate laneStepRate (const Lane& lane) noexcept
{
    return lane.triplet ? LaneStepRate { 4, 3 } : LaneStepRate { 1, 1 };
}

/** The lane length a fresh straight/triplet lane should have (one bar of its own steps). */
inline constexpr int defaultLaneLength (bool triplet) noexcept
{
    return triplet ? tripletStepsPerBar : straightStepsPerBar;
}

/** ceil(a / b) for a >= 0, b > 0. */
inline std::int64_t ceilDivNonNegative (std::int64_t a, std::int64_t b) noexcept
{
    return (a + b - 1) / b;
}

/** The half-open range [first, end) of this lane's own step indices that BEGIN inside
    global 1/16 step `stepIndex`. Pure integer maths, so which steps fire is independent
    of the audio block size — that is what keeps a triplet lane drift-free.

    A straight lane always yields exactly {stepIndex}. A 1/8-triplet lane yields one step
    for three global steps out of four, and none for the fourth. */
inline void laneStepsInGlobalStep (const Lane& lane, std::int64_t stepIndex,
                                   std::int64_t& first, std::int64_t& end) noexcept
{
    const auto rate = laneStepRate (lane);
    first = ceilDivNonNegative (stepIndex * rate.den, rate.num);
    end   = ceilDivNonNegative ((stepIndex + 1) * rate.den, rate.num);
}

/** Where lane step `laneStep` sits inside global step `stepIndex`, in units of a 1/16
    step. Always in [0, 1), and exactly 0 for a straight lane. */
inline double laneStepOffset (const Lane& lane, std::int64_t laneStep, std::int64_t stepIndex) noexcept
{
    const auto rate = laneStepRate (lane);
    return (double) (laneStep * rate.num - stepIndex * rate.den) / (double) rate.den;
}

/** The lane's own step index sounding at (or most recently before) global step
    `stepIndex` — the playhead position for that lane. */
inline std::int64_t laneStepAtGlobalStep (const Lane& lane, std::int64_t stepIndex) noexcept
{
    const auto rate = laneStepRate (lane);
    return (stepIndex * rate.den) / rate.num;   // floor; stepIndex >= 0 while playing
}

} // namespace rollforge
