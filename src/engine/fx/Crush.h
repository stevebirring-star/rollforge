#pragma once

// RollForge — Crush: bit-depth reduction + sample-rate reduction (sample & hold) +
// a soft clip, dry/wet by a single 0..1 amount (0 = bypass). One of the four
// Phase-4 macros. Hand-rolled (no juce::dsp) -> headless-testable. RT-safe.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class Crush
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void  setAmount (float amount) noexcept { target.store (amount, std::memory_order_release); }
    float getAmount() const noexcept { return target.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maxCh = 8;

    std::atomic<float> target { 0.0f };
    float held[maxCh]   = {};       // per-channel sample-and-hold value
    int   holdCounter   = 0;        // shared downsample phase
};

} // namespace rollforge
