#pragma once

// RollForge — MasterLimiter: an always-on brickwall peak limiter on the master
// bus. A peak follower with instant attack + smoothed release computes the gain
// reduction, and a hard ceiling clamp guarantees the output magnitude never
// exceeds the ceiling. Hand-rolled (no juce::dsp) so it is headless-testable.
//
// ENGINE LAYER RULE: no JUCE GUI includes. RT-safe: no alloc/lock/IO in process().

#include <juce_audio_basics/juce_audio_basics.h>

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

private:
    double sampleRate   = 44100.0;
    float  ceiling      = 0.98f;    // ~ -0.18 dBFS
    float  envelope     = 0.0f;     // peak follower
    float  releaseCoeff = 0.0f;     // one-pole release smoothing
};

} // namespace rollforge
