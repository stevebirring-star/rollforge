#pragma once

// RollForge — Drive: soft saturation (tanh waveshaper) plus a gentle high-shelf
// brighten, dry/wet by a single 0..1 amount (0 = bypass). One of the four Phase-4
// macros. Hand-rolled (no juce::dsp) -> headless-testable. RT-safe.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class Drive
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** 0..1 macro amount; 0 = bypass. Message-thread safe. */
    void  setAmount (float amount) noexcept { target.store (amount, std::memory_order_release); }
    float getAmount() const noexcept { return target.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maxCh = 8;

    double sampleRate = 44100.0;
    float  lpCoeff    = 0.0f;         // one-pole coeff for the shelf's LP reference
    float  lp[maxCh]  = {};           // per-channel LP state
    std::atomic<float> target { 0.0f };
};

} // namespace rollforge
