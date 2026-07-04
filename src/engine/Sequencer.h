#pragma once

// RollForge — Sequencer: drives the DrumEngine from a Pattern + Clock.
//
// Each audio block it reads the latest Pattern snapshot (lock-free, via
// TripleBuffer), advances the Clock, and at every 1/16 step it fires the active
// lanes' pads into the DrumEngine at the EXACT sample offset — rendering the
// block in segments split at those offsets so triggers are sample-accurate (not
// quantised to the block boundary).
//
// THREADING:
//   * setPattern/setPlaying/setTempo/requestReset — MESSAGE thread. Pattern edits
//     publish a snapshot; transport changes are marshaled to the audio thread via
//     atomics and applied at the next block.
//   * process() — AUDIO thread. No allocation, locking, or IO.
//
// Straight 1/16 lanes only for now; per-lane triplet, ratchets, probability and
// micro-shift arrive in the next commit.
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
    void fireStep (DrumEngine& engine, const Pattern& pattern, std::int64_t stepIndex) noexcept;

    Clock                 clock;
    TripleBuffer<Pattern> patterns;

    std::atomic<bool>   playing        { false };
    std::atomic<double> pendingTempo   { 120.0 };
    std::atomic<bool>   tempoDirty     { false };
    std::atomic<bool>   resetRequested { false };

    std::atomic<std::int64_t> currentStep  { -1 };
    std::atomic<std::int64_t> triggerCount { 0 };
};

} // namespace rollforge
