#pragma once

// RollForge — Capture: human timing in, a Pattern out.
//
// One quantiser, two sources. Tapping the pads while the loop runs and beatboxing into a
// microphone are the same problem once each has produced a list of "at 0.62 seconds, this
// drum, this hard" — so they share this, and neither gets to round differently from the other.
//
// WHAT IT ROUNDS TO. Each lane owns its own grid: a triplet lane has twelve steps to the bar,
// a straight one sixteen. A hit is quantised against the lane it lands in, not against some
// global step, or a triplet hat tapped in time would be dragged onto the nearest 1/16.
//
// THE WRAP IS NOT AN EDGE CASE, IT IS THE POINT. Nobody taps a downbeat late. They tap it
// early, a few milliseconds before the loop turns over, and a quantiser that clamps to the
// last step puts their kick at the END of the bar instead of the start of it. Rounding past
// the last step wraps to step 0, and rounding before step 0 wraps to the last step.
//
// MODEL LAYER RULE: no JUCE GUI. Pure, headless-testable.

#include "model/Pattern.h"

#include <vector>

namespace rollforge
{
namespace Capture
{
    /** One drum hit, timed from the start of the loop. `seconds` may exceed one loop or fall
        slightly below zero; both wrap. */
    struct Hit
    {
        double seconds  = 0.0;
        int    pad      = 0;
        float  velocity = 1.0f;
    };

    struct Options
    {
        double bpm         = 120.0;
        float  minVelocity = 0.05f;   // below this a hit is noise, not a note
        bool   replace     = false;   // false = overdub onto what is already there
    };

    struct Result
    {
        int placed  = 0;   // steps that were off and are now on
        int merged  = 0;   // hits that landed on a step already on (the louder one wins)
        int dropped = 0;   // too quiet, or no lane plays that pad
    };

    /** Which lane plays `pad`, or -1. The first match wins; a pad with no lane cannot be
        captured onto. */
    int laneForPad (const Pattern& pattern, int pad) noexcept;

    /** Rounds `hit` onto its lane's grid. False when no lane plays that pad. `stepOut` is
        always inside the lane's length. */
    bool quantise (const Pattern& pattern, const Hit& hit, double bpm,
                   int& laneOut, int& stepOut) noexcept;

    /** The step a captured hit leaves behind, given whatever was already on that step. Shared
        by the batch path (apply) and by the live tap path in the UI, so the two cannot drift:
        a captured hit is a CLEAN single tap, and two taps on one step are one note at the
        louder velocity. */
    Step merge (const Step& existing, float velocity) noexcept;

    /** Quantises every hit into `pattern`. A captured hit is a CLEAN single tap: it clears
        the ratchets, probability and micro-shift of whatever step it lands on, because you
        tapped a note, not a roll someone else programmed there. */
    Result apply (Pattern& pattern, const std::vector<Hit>& hits, const Options& options);
}
} // namespace rollforge
