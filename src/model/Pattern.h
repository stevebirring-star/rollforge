#pragma once

// RollForge — Pattern: a set of lanes plus tempo/swing (pure data).
//
// MODEL LAYER RULE: no JUCE GUI includes. Fixed-capacity (maxLanes) and fully
// value-copyable, so a whole Pattern can be published to the audio thread as an
// immutable snapshot via engine/TripleBuffer.h (the audio thread reads it, never
// mutates it, and never blocks).

#include "model/Lane.h"
#include "model/RollRegion.h"

#include <array>
#include <cmath>

namespace rollforge
{

inline constexpr int maxLanes = 16;
inline constexpr int maxRolls = 16;

struct Pattern
{
    std::array<Lane, maxLanes> lanes {};
    int    numLanes = 0;         // active lanes, 0..maxLanes
    double bpm      = 120.0;
    float  swing    = 0.0f;      // 0..1 (delay applied to off-beat 8ths)

    std::array<CompiledRoll, maxRolls> rolls {};   // pre-compiled roll overlays
    int    numRolls = 0;         // active rolls, 0..maxRolls

    Lane&       lane (int index)       noexcept { return lanes[(std::size_t) index]; }
    const Lane& lane (int index) const noexcept { return lanes[(std::size_t) index]; }
};

/** An empty but PLAYABLE pattern: sixteen lanes, lane i firing pad i, one bar of straight
    1/16ths, nothing lit. A default-constructed Pattern has no lanes at all, so its steps
    would light in the grid and never sound — which is exactly what a freshly cleared slot
    must not do. Pure. */
inline Pattern blankPattern() noexcept
{
    Pattern p;
    p.numLanes = maxLanes;
    for (int i = 0; i < maxLanes; ++i)
    {
        p.lane (i).targetPad = i;
        p.lane (i).length    = straightStepsPerBar;
        p.lane (i).triplet   = false;
    }
    return p;
}

/** The same pattern with nothing written in it: every step off and every roll gone, but the
    lane layout (target pads, lengths, triplet flags) and the tempo/swing it was written at
    left alone — clearing the notes must not silently retune the slot to 120 BPM straight.
    A pattern with no lanes at all becomes a blankPattern(), since a lane-less pattern would
    light steps in the grid that could never sound. Pure. */
inline Pattern clearedPattern (const Pattern& p) noexcept
{
    if (p.numLanes <= 0)
        return blankPattern();

    Pattern out = p;
    out.numRolls = 0;

    const int lanes = out.numLanes > maxLanes ? maxLanes : out.numLanes;
    for (int i = 0; i < lanes; ++i)
        for (int s = 0; s < maxStepsPerLane; ++s)
            out.lane (i).step (s) = Step {};

    return out;
}

/** Where a roll sits on the step grid. `lane` is -1 when there is nothing to draw:
    either the index names no roll, the roll compiled to no hits, or no active lane
    fires its target pad. */
struct RollExtent
{
    int lane        = -1;
    int startStep   = 0;
    int lengthSteps = 1;
};

/** The grid block roll `rollIndex` occupies. A roll knows only its target PAD, so the
    lane is the first active one firing that pad; the span comes from the compiled hits
    rather than the region that made them, which is what lets a GENERATED roll (from
    FillEngine) draw in the same place it sounds, without widening CompiledRoll or the
    saved project format. Pure. */
inline RollExtent rollExtent (const Pattern& p, int rollIndex) noexcept
{
    RollExtent e;

    const int rolls = p.numRolls < 0 ? 0 : (p.numRolls > maxRolls ? maxRolls : p.numRolls);
    if (rollIndex < 0 || rollIndex >= rolls)
        return e;

    const CompiledRoll& r = p.rolls[(std::size_t) rollIndex];

    const int count = r.count < 0 ? 0 : (r.count > maxRollEvents ? maxRollEvents : r.count);
    if (count == 0)
        return e;   // a roll that makes no sound draws nothing

    const int lanes = p.numLanes < 0 ? 0 : (p.numLanes > maxLanes ? maxLanes : p.numLanes);
    for (int i = 0; i < lanes; ++i)
        if (p.lane (i).targetPad == r.targetPad)
        {
            e.lane = i;
            break;
        }

    if (e.lane < 0)
        return e;

    const int laneLen = p.lane (e.lane).length < 1 ? 1
                      : (p.lane (e.lane).length > maxStepsPerLane ? maxStepsPerLane
                                                                  : p.lane (e.lane).length);

    e.startStep = (int) std::lround ((double) r.startStep);
    if (e.startStep < 0)           e.startStep = 0;
    if (e.startStep > laneLen - 1) e.startStep = laneLen - 1;

    float last = 0.0f;
    for (int k = 0; k < count; ++k)
        if (r.events[(std::size_t) k].stepOffset > last)
            last = r.events[(std::size_t) k].stepOffset;

    // Hits land in columns 0..floor(last) relative to the start, so the block is that
    // many columns plus one wide.
    e.lengthSteps = (int) std::floor ((double) last) + 1;
    if (e.lengthSteps < 1)
        e.lengthSteps = 1;
    if (e.startStep + e.lengthSteps > laneLen)
        e.lengthSteps = laneLen - e.startStep;

    return e;
}

/** True when nothing would sound: no lit step in any active lane, and no roll. The slot
    strip uses this to tell an empty pattern slot from a written one, so "A" and "E" look
    different before you've heard either. Pure. */
inline bool patternIsEmpty (const Pattern& p) noexcept
{
    if (p.numRolls > 0)
        return false;

    const int lanes = p.numLanes < 0 ? 0 : (p.numLanes > maxLanes ? maxLanes : p.numLanes);
    for (int i = 0; i < lanes; ++i)
    {
        const Lane& lane = p.lane (i);
        const int len = lane.length < 0 ? 0 : (lane.length > maxStepsPerLane ? maxStepsPerLane : lane.length);
        for (int s = 0; s < len; ++s)
            if (lane.step (s).on)
                return false;
    }
    return true;
}

/** The pattern's natural length in whole bars (1 bar = 16 global 1/16 steps), taken from
    the longest active lane and always >= 1. Exports use this to size the render so a
    multi-bar pattern isn't truncated. Pure — no JUCE, headless-testable.

    A lane's `length` counts ITS OWN steps, which for a triplet lane are 4/3 of a 1/16
    each: 12 triplet steps are one bar, not three quarters of one. So the length is
    converted to global steps before the bars are counted. Straight lanes convert 1:1. */
inline int patternBars (const Pattern& p) noexcept
{
    int lanes = p.numLanes;
    if (lanes < 0)        lanes = 0;
    if (lanes > maxLanes) lanes = maxLanes;

    int maxGlobalSteps = 16;   // never report fewer than one bar
    for (int i = 0; i < lanes; ++i)
    {
        const Lane& lane = p.lane (i);
        int len = lane.length;
        if (len < 0)                len = 0;
        if (len > maxStepsPerLane)  len = maxStepsPerLane;

        // ceil(len * num / den): the global steps this lane needs to play through once.
        const auto rate = laneStepRate (lane);
        const int globalSteps = (len * rate.num + rate.den - 1) / rate.den;
        if (globalSteps > maxGlobalSteps)
            maxGlobalSteps = globalSteps;
    }
    return (maxGlobalSteps + 15) / 16;   // ceil to whole bars (>= 1)
}

} // namespace rollforge
