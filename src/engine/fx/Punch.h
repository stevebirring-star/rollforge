#pragma once

// RollForge — Punch: transient emphasis (fast vs slow envelope difference boosts
// attacks) blended with a little parallel saturation for body, dry/wet by a single
// 0..1 amount (0 = bypass). One of the four Phase-4 macros. Hand-rolled (no
// juce::dsp) -> headless-testable. RT-safe.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class Punch
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void  setAmount (float amount) noexcept { target.store (amount, std::memory_order_release); }
    float getAmount() const noexcept { return target.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maxCh = 8;

    double sampleRate = 44100.0;
    float  fastAtt = 0.0f, fastRel = 0.0f, slowAtt = 0.0f, slowRel = 0.0f;
    float  envFast[maxCh] = {};
    float  envSlow[maxCh] = {};
    std::atomic<float> target { 0.0f };
};

} // namespace rollforge
