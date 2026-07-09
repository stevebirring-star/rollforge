#pragma once

// RollForge — MasterLimiter: an always-on brickwall peak limiter on the master
// bus. A peak follower with instant attack + smoothed release computes the gain
// reduction, and a hard ceiling clamp guarantees the output magnitude never
// exceeds the ceiling. Hand-rolled (no juce::dsp) so it is headless-testable.
//
// It also reports the peak it was HANDED, before it clamped anything. That is the only
// number from which a CLIP lamp can mean something: the output cannot exceed the ceiling by
// construction, so a lamp watching the output is a lamp that can never light. What a user
// wants to know is "did the master go over, and the limiter save me?" — which is a fact about
// the limiter's INPUT.
//
// ENGINE LAYER RULE: no JUCE GUI includes. RT-safe: no alloc/lock/IO in process().

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class MasterLimiter
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Limits `buffer` in place so |sample| <= ceiling. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getCeiling() const noexcept { return ceiling; }

    /** The largest |sample| handed to process() since this was last called, then zeroed.
        Any thread; the meter reads it, the audio thread writes it. A value >= 1.0 means the
        master went over full scale and only the limiter stopped it. */
    float readAndResetInputPeak() noexcept { return inputPeak.exchange (0.0f, std::memory_order_acq_rel); }

private:
    double sampleRate   = 44100.0;
    float  ceiling      = 0.98f;    // ~ -0.18 dBFS
    float  envelope     = 0.0f;     // peak follower
    float  releaseCoeff = 0.0f;     // one-pole release smoothing

    // Written by the audio thread, drained by whoever is showing a clip lamp. Not reset by
    // reset(): a clip that happened is a fact, and a device change should not erase it.
    std::atomic<float> inputPeak { 0.0f };
};

} // namespace rollforge
