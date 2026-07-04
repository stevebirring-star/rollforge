#pragma once

// RollForge — RollRegion: a non-destructive "roll" (a burst of repeated hits on
// one pad over a span, with an accelerating/decelerating rate and per-hit
// Speed/Volume/Pitch shaping). RollCompiler turns it into a CompiledRoll.
//
// This is the Phase-3 differentiator's core data. Pure (no JUCE), so it and the
// compiler are fully headless-unit-tested. Roll events use TEMPO-INDEPENDENT step
// offsets: the compiler runs once on the message thread, and the audio thread just
// scales step -> sample at play time (no RT-thread compilation).

#include <array>

namespace rollforge
{

/** A start->end value eased by `shape`: 0 = linear, >0 = slow-start/fast-end
    (accelerating), <0 = fast-start/slow-end. The exponent is 2^(2*shape). */
struct RollCurve
{
    float start = 0.0f;
    float end   = 0.0f;
    float shape = 0.0f;   // -1..+1
};

struct RollRegion
{
    double startStep   = 0.0;   // roll start, in step (1/16) units within the pattern
    double lengthSteps = 4.0;   // roll span, in steps
    int    targetPad   = 0;

    // Speed = hits per step (the roll's rate); Volume = velocity; Pitch = semitones.
    RollCurve speed  { 2.0f, 4.0f, 0.0f };   // accelerate 1/8 -> 1/16 by default
    RollCurve volume { 1.0f, 0.6f, 0.0f };   // fade out a touch
    RollCurve pitch  { 0.0f, 0.0f, 0.0f };
};

/** One compiled hit of a roll. `stepOffset` is in STEPS relative to the roll's
    start (tempo-independent; the Sequencer multiplies by samplesPerStep and adds
    the roll's absolute start sample). */
struct RollEvent
{
    float stepOffset     = 0.0f;
    float velocity       = 1.0f;
    float pitchSemitones = 0.0f;
};

/** Max hits a compiled roll holds (excess is dropped by the compiler). */
inline constexpr int maxRollEvents = 128;

/** A roll compiled to tempo-independent step-offset events, ready for the audio
    thread. Trivially copyable, so it rides inside a Pattern snapshot. */
struct CompiledRoll
{
    int   targetPad = 0;
    float startStep = 0.0f;   // step-in-loop where the roll begins
    int   count     = 0;
    std::array<RollEvent, maxRollEvents> events {};
};

} // namespace rollforge
