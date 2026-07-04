#pragma once

// RollForge — Sequencer: drives the DrumEngine from a Pattern + Clock.
//
// Each audio block it reads the latest Pattern snapshot (lock-free, via
// TripleBuffer), advances the Clock, and turns each active step into one or more
// note events at EXACT sample positions:
//   * probability — a deterministic per-(step,lane) hash gates whether the step
//     fires (so a loop is reproducible but varies bar to bar);
//   * micro-shift — moves the step's events LATER by up to half a step (forward
//     only for now; backward "rush" needs a look-ahead pass and is reserved, so a
//     negative microShift is clamped to 0);
//   * ratchets    — 1..8 evenly-spaced sub-hits across the step, with a velocity
//     ramp.
// Events are queued by absolute sample in a fixed pending buffer (so a ratchet or
// forward shift that crosses a block boundary still fires at the right sample),
// then the block is rendered in segments split at the event offsets — fully
// sample-accurate.
//
// NOT YET: per-lane triplet timing (needs lane timing decoupled from the global
// 1/16 grid) — a later commit.
//
// THREADING:
//   * setPattern/setPlaying/setTempo/requestReset — MESSAGE thread (pattern edits
//     publish a snapshot; transport changes marshaled to the audio thread).
//   * process() — AUDIO thread. No allocation, locking, or IO.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/Clock.h"
#include "engine/DrumEngine.h"
#include "engine/TripleBuffer.h"
#include "model/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>

namespace rollforge
{

class Sequencer
{
public:
    Sequencer();   // publishes an empty starting pattern

    /** Prepares the clock for the sample rate. Does NOT touch the pattern, so a
        device restart keeps whatever pattern was set. */
    void prepare (double sampleRate) noexcept;

    //==============================================================================
    // Message-thread control.
    void setPattern (const Pattern& pattern);   // publishes a snapshot
    void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay, std::memory_order_release); }
    void setTempo (double bpm) noexcept;
    void requestReset() noexcept { resetRequested.store (true, std::memory_order_release); }

    //==============================================================================
    /** Audio thread: renders `buffer` (assumed pre-cleared), driving `engine` with
        sample-accurate sequenced triggers plus the engine's own queued UI/MIDI
        triggers. */
    void process (DrumEngine& engine, juce::AudioBuffer<float>& buffer) noexcept;

    //==============================================================================
    // Telemetry (any thread).
    bool         isPlaying()       const noexcept { return playing.load (std::memory_order_acquire); }
    std::int64_t getCurrentStep()  const noexcept { return currentStep.load (std::memory_order_acquire); }
    std::int64_t getTriggerCount() const noexcept { return triggerCount.load (std::memory_order_acquire); }

private:
    struct Event
    {
        std::int64_t sample;    // absolute transport sample
        int          pad;
        float        velocity;
    };

    // Sized well above a realistic worst case (steps-in-block x lanes x ratchets
    // + ratchet carry-over); events beyond it are dropped gracefully (addEvent).
    static constexpr int maxPendingEvents = 2048;

    void generateStepEvents (const Pattern& pattern, std::int64_t stepIndex, std::int64_t stepSample) noexcept;
    void addEvent (std::int64_t sample, int pad, float velocity) noexcept;
    void renderWithEvents (DrumEngine& engine, juce::AudioBuffer<float>& buffer,
                           std::int64_t blockStart, int numSamples) noexcept;

    Clock                 clock;
    TripleBuffer<Pattern> patterns;

    Event pending[maxPendingEvents];
    int   pendingCount = 0;

    std::atomic<bool>   playing        { false };
    std::atomic<double> pendingTempo   { 120.0 };
    std::atomic<bool>   tempoDirty     { false };
    std::atomic<bool>   resetRequested { false };

    std::atomic<std::int64_t> currentStep  { -1 };
    std::atomic<std::int64_t> triggerCount { 0 };
};

} // namespace rollforge
