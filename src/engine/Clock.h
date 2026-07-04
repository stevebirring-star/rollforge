#pragma once

// RollForge — Clock: sample-accurate 1/16-step scheduler.
//
// Turns BPM + block-based audio processing into exact per-block sample offsets
// for each 1/16 step. DRIFT-FREE across buffer sizes: step N always lands at
// baseSample + llround((N - baseStep) * samplesPerStep), computed from the
// absolute step index each block — so a step near a block boundary fires in
// whichever block contains its absolute sample, and 64/256/1024-sample buffers
// all produce identical step positions.
//
// (That buffer-size invariance holds for a CONSTANT tempo. A mid-play setTempo
// rebases at the block boundary where it is applied; since that boundary lands at
// different absolute samples for different buffer sizes, positions after a
// mid-play tempo change can diverge between buffer sizes. When and where a tempo
// change is applied is the Sequencer's call, not the Clock's.)
//
// Per-lane triplet + length are applied by the Sequencer (it maps this global
// 1/16 step index onto each lane). The Clock only owns the global grid.
//
// THREADING: processBlock() and the setters (setTempo/setPlaying/reset) run on
// the SAME thread (the audio thread). The Sequencer marshals UI transport/tempo
// changes onto the audio thread (via the command queue) before calling these.
//
// ENGINE LAYER RULE: no JUCE includes (pure C++).

#include <cstdint>

namespace rollforge
{

class Clock
{
public:
    Clock() { setTempo (bpm); }

    /** Sets the device sample rate and resets to step 0. */
    void prepare (double sampleRate) noexcept;

    /** Sets the tempo. Mid-play this rebases the grid so the NEXT step keeps its
        scheduled position and later steps are respaced at the new tempo (no jump). */
    void setTempo (double newBpm) noexcept;
    double getTempo() const noexcept { return bpm; }

    void setPlaying (bool shouldPlay) noexcept { playing = shouldPlay; }
    bool isPlaying() const noexcept { return playing; }

    /** Rewinds the playhead to step 0. */
    void reset() noexcept;

    /** Advances one audio block. Calls `onStep(int64 stepIndex, int offsetInBlock)`
        for every 1/16 step boundary that lands in [0, numSamples), in order.
        No-op when stopped. Audio-thread, allocation/lock-free. */
    template <typename Fn>
    void processBlock (int numSamples, Fn&& onStep) noexcept
    {
        if (! playing || numSamples <= 0)
            return;

        const std::int64_t blockEnd = sampleCounter + numSamples;

        for (std::int64_t ss = stepSampleOf (nextStepIndex);
             ss < blockEnd;
             ss = stepSampleOf (nextStepIndex))
        {
            const std::int64_t offset = ss - sampleCounter;
            onStep (nextStepIndex, (int) (offset < 0 ? 0 : offset));
            ++nextStepIndex;
        }

        sampleCounter = blockEnd;
    }

    // Telemetry (audio thread; the Sequencer republishes for the UI via an atomic).
    std::int64_t getStepCounter()   const noexcept { return nextStepIndex; }
    std::int64_t getSampleCounter() const noexcept { return sampleCounter; }
    double       getSamplesPerStep() const noexcept { return samplesPerStep; }

private:
    std::int64_t stepSampleOf (std::int64_t index) const noexcept;

    double sampleRate     = 44100.0;
    double bpm            = 120.0;
    double samplesPerStep = 0.0;    // one 1/16 note, in samples
    bool   playing        = false;

    std::int64_t sampleCounter = 0; // absolute sample at the current block start
    std::int64_t nextStepIndex = 0; // next step to fire
    std::int64_t baseStep      = 0; // rebase origin (step index)
    std::int64_t baseSample    = 0; // rebase origin (absolute sample)
};

} // namespace rollforge
