#pragma once

// RollForge — Song: an arrangement over the A..H bank.
//
// A chain of steps, each one "play slot X for N bars". The whole of song mode reduces to a
// single question asked once per bar — "which slot should be sounding at bar B?" — and this
// answers it. The app queues the answer for bar B+1 a bar early, so the Sequencer's existing
// bar-line switch lands it exactly on time and nothing has to be scheduled from the audio
// thread.
//
// A step of zero or negative bars is treated as one bar rather than skipped: a chain that
// silently swallowed a step would be far more confusing than one that plays it briefly, and
// nothing in the UI can produce a zero anyway.
//
// MODEL LAYER RULE: no JUCE GUI. Pure, headless-testable, value-copyable.

#include "model/PatternBank.h"

#include <vector>

namespace rollforge
{

struct SongStep
{
    int slot = 0;   // 0..numPatternSlots-1
    int bars = 1;   // >= 1 (a smaller value is read as 1)
};

struct Song
{
    std::vector<SongStep> steps;
    bool loop = true;

    bool isEmpty() const noexcept { return steps.empty(); }

    /** How many bars one pass through the chain lasts. Zero for an empty song. */
    int totalBars() const noexcept;

    /** Index into `steps` of the step covering `bar`, or -1 when the song is empty, `bar`
        is negative, or the song has run out (only possible when `loop` is false). */
    int stepAtBar (int bar) const noexcept;

    /** The slot sounding at `bar`, or -1 on the same conditions as stepAtBar(). */
    int slotAtBar (int bar) const noexcept;
};

} // namespace rollforge
