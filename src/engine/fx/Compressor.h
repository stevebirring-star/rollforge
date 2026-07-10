#pragma once

// RollForge — Compressor: a one-knob "glue" bus compressor. A linked-stereo peak
// detector with fixed musical attack/release drives a feed-forward gain computer;
// a single 0..1 amount lowers the threshold, raises the ratio, and adds partial
// auto makeup. 0 = bypass. Hand-rolled (no juce::dsp) so it is headless-testable.
// RT-safe: no alloc/lock/IO in process().
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class Compressor
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** 0..1 amount; 0 = bypass. Message-thread safe. */
    void  setAmount (float amount) noexcept { target.store (amount, std::memory_order_release); }
    float getAmount() const noexcept { return target.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maxCh = 8;

    double sampleRate   = 44100.0;
    float  attackCoeff  = 0.0f;    // one-pole rise coeff (~10 ms)
    float  releaseCoeff = 0.0f;    // one-pole fall coeff (~120 ms)
    float  envelope     = 0.0f;    // linked peak follower

    std::atomic<float> target { 0.0f };
};

} // namespace rollforge
