#pragma once

// RollForge — Sequencer: drives the DrumEngine from a Pattern + Clock.
//
// Each audio block it advances the Clock (emitting 1/16 step boundaries) and
// turns each active step into sample-accurate note events:
//   * probability — a deterministic per-(step,lane) hash gates whether the step
//     fires (so a loop is reproducible but varies bar to bar);
//   * micro-shift — moves the step's events LATER by up to half a step (forward
//     only for now; backward "rush" needs a look-ahead pass and is reserved, so a
//     negative microShift is clamped to 0);
//   * ratchets    — 1..8 evenly-spaced sub-hits across the step, with a velocity
//     ramp.
// Events are queued by absolute sample in a fixed pending buffer (so an event
// crossing a block boundary still fires at the right sample), then the block is
// rendered in segments split at the event offsets — fully sample-accurate.
//
// PATTERN SWITCHING: setPattern() replaces the active pattern immediately;
// queuePattern() swaps it in at the next bar boundary while playing (immediately
// when stopped) for a glitch-free A->B change. A "bar" is a fixed 16 steps (4/4 @
// 1/16) for now.
//
// NOT YET: per-lane triplet timing (needs lane timing decoupled from the 1/16 grid).
//
// THREADING:
//   * setPattern/queuePattern/setPlaying/setTempo/requestReset — MESSAGE thread.
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
#include <memory>
#include <vector>

namespace rollforge
{

class Sequencer
{
public:
    Sequencer();   // allocates the (large) pattern state on the heap

    /** Prepares the clock for the sample rate. Does NOT touch the pattern, so a
        device restart keeps whatever pattern was set. */
    void prepare (double sampleRate) noexcept;

    //==============================================================================
    // Message-thread control.
    void setPattern (const Pattern& pattern);     // replace the active pattern now
    void queuePattern (const Pattern& pattern);   // swap in at the next bar (glitch-free)
    void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay, std::memory_order_release); }
    void setTempo (double bpm) noexcept;
    /** Swing 0..1: delays the off-beat (2nd of each pair) 1/16 by swing/3 of a
        step, so 1 gives a ~66:33 shuffle. A global groove control. */
    void setSwing (float amount) noexcept { swing.store (amount, std::memory_order_release); }
    /** Humanise 0..1: seeded per-event forward timing + velocity jitter applied at
        playback (non-destructive — the pattern is untouched). 0 = robotic, 1 = loose. */
    void setHumanise (float amount) noexcept { humanise.store (amount, std::memory_order_release); }
    void requestReset() noexcept { resetRequested.store (true, std::memory_order_release); }

    //==============================================================================
    /** Audio thread: renders `buffer` (assumed pre-cleared), driving `engine` with
        sample-accurate sequenced triggers plus the engine's own queued UI/MIDI
        triggers. */
    void process (DrumEngine& engine, juce::AudioBuffer<float>& buffer) noexcept;

    //==============================================================================
    // Telemetry (any thread).
    bool         isPlaying()       const noexcept { return playing.load (std::memory_order_acquire); }
    float        getSwing()        const noexcept { return swing.load (std::memory_order_acquire); }
    float        getHumanise()     const noexcept { return humanise.load (std::memory_order_acquire); }
    double       getTempo()        const noexcept { return pendingTempo.load (std::memory_order_acquire); }
    bool         isSwitchQueued()  const noexcept { return switchQueued.load (std::memory_order_acquire); }

    /** The transport position at the START of the block currently being rendered, in samples
        since Play was pressed (the clock is reset then). Zero while stopped is not meaningful;
        check isPlaying(). Capture timestamps its hits against this, so a tap and a beatboxed
        hit are measured on the same clock the sequencer plays to -- a wall clock would drift
        against the audio device. */
    std::int64_t getTransportSamples() const noexcept { return transportSamples.load (std::memory_order_acquire); }

    /** The bar the queued switch quantises to, in global 1/16 steps. Song mode counts bars
        off getCurrentStep() and must use the same number the switch does, not its own 16. */
    static constexpr int getBarSteps() noexcept { return barLengthSteps; }

    /** Counts the queued patterns that have actually BECOME the active one. A caller that
        wants to know "has my queued switch landed yet?" must compare this against the value
        it read when it queued, not watch isSwitchQueued() fall: that flag is still false in
        the gap between queuePattern() and the next audio block, and it can be raised and
        lowered inside a single block when the switch is queued right on a bar line. */
    std::int64_t getSwitchCount() const noexcept { return switchCount.load (std::memory_order_acquire); }
    std::int64_t getCurrentStep()  const noexcept { return currentStep.load (std::memory_order_acquire); }
    std::int64_t getTriggerCount() const noexcept { return triggerCount.load (std::memory_order_acquire); }

private:
    struct Event
    {
        std::int64_t sample;      // absolute transport sample
        int          pad;
        float        velocity;
        float        pitchOffset; // extra semitones (rolls); 0 for plain steps
        int          sampleLock;  // -1 = let the pad pick its layer; else pin that one
    };

    // A bar for switch quantisation = 16 steps (4/4 at 1/16). Configurable later.
    // Public read access is via getBarSteps().
    static constexpr int barLengthSteps = 16;

    // Sized well above a realistic worst case (steps-in-block x lanes x ratchets
    // + ratchet carry-over); events beyond it are dropped gracefully (addEvent).
    static constexpr int maxPendingEvents = 2048;

    void applyIncomingPattern (bool nowPlaying) noexcept;
    void generateStepEvents (std::int64_t stepIndex, std::int64_t stepSample) noexcept;
    void addEvent (std::int64_t sample, int pad, float velocity, float pitchOffset = 0.0f,
                   int sampleLock = -1) noexcept;
    void renderWithEvents (DrumEngine& engine, juce::AudioBuffer<float>& buffer,
                           std::int64_t blockStart, int numSamples) noexcept;

    Clock clock;

    // Pattern hand-off: the message thread publishes into `incoming` and sets a
    // mode; the audio thread copies it into the owned `active` pattern (now, or at
    // the next bar for a queued switch). `active`/`queued` are audio-thread-owned.
    // Heap-allocated: a Pattern is large (lanes + compiled rolls), so keeping 5 of
    // them by value would overflow a small (Windows 1 MB) stack when a Sequencer is
    // a stack local. Allocated once in the ctor; process() never allocates.
    std::unique_ptr<TripleBuffer<Pattern>> incoming;
    std::unique_ptr<Pattern>               active;
    std::unique_ptr<Pattern>               queued;
    bool                                   hasQueued = false;
    std::atomic<int>                       incomingMode { 0 };   // 0 none, 1 immediate, 2 queued

    std::vector<Event> pending;              // sized to maxPendingEvents in the ctor
    int                pendingCount = 0;

    std::atomic<bool>   playing        { false };
    std::atomic<float>  swing          { 0.0f };
    std::atomic<float>  humanise       { 0.0f };
    std::atomic<double> pendingTempo   { 120.0 };
    std::atomic<bool>   tempoDirty     { false };
    std::atomic<bool>   resetRequested { false };
    std::atomic<bool>   switchQueued   { false };

    std::atomic<std::int64_t> switchCount      { 0 };
    std::atomic<std::int64_t> transportSamples { 0 };
    std::atomic<std::int64_t> currentStep  { -1 };
    std::atomic<std::int64_t> triggerCount { 0 };
};

} // namespace rollforge
