#pragma once

// RollForge — MasterEq: a 3-band master EQ (low shelf ~100 Hz, mid peak ~1 kHz,
// high shelf ~6 kHz), each +/-12 dB. Hand-rolled RBJ biquads (no juce::dsp) so it
// is headless-testable. 0 dB on all three bands = clean pass-through (bypass).
// RT-safe: no alloc/lock/IO in process(); coeffs recompute (cheaply) only when a
// band's gain changes, off the atomic gain values, so there is no coeff data race.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace rollforge
{

class MasterEq
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Band gains in dB (nominally -12..+12); 0 = flat. Message-thread safe.
    void  setLowDb  (float db) noexcept { lowDb.store  (db, std::memory_order_release); }
    void  setMidDb  (float db) noexcept { midDb.store  (db, std::memory_order_release); }
    void  setHighDb (float db) noexcept { highDb.store (db, std::memory_order_release); }
    float getLowDb()  const noexcept { return lowDb.load  (std::memory_order_acquire); }
    float getMidDb()  const noexcept { return midDb.load  (std::memory_order_acquire); }
    float getHighDb() const noexcept { return highDb.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr int maxCh = 8;

    // A biquad in Direct Form I with per-channel state (a0 already normalised to 1).
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1[maxCh] = {}, x2[maxCh] = {}, y1[maxCh] = {}, y2[maxCh] = {};

        float processSample (int ch, float x) noexcept
        {
            const float y = b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
            x2[ch] = x1[ch]; x1[ch] = x;
            y2[ch] = y1[ch]; y1[ch] = y;
            return y;
        }

        void resetState() noexcept
        {
            for (int c = 0; c < maxCh; ++c) { x1[c] = x2[c] = y1[c] = y2[c] = 0.0f; }
        }
    };

    void setLowShelf  (Biquad&, double fc, double gainDb) noexcept;
    void setPeak      (Biquad&, double fc, double q, double gainDb) noexcept;
    void setHighShelf (Biquad&, double fc, double gainDb) noexcept;

    double sampleRate = 44100.0;
    Biquad low, mid, high;

    // Last-applied gains, so coeffs recompute only when a band actually changes.
    float lastLow = 0.0f, lastMid = 0.0f, lastHigh = 0.0f;

    std::atomic<float> lowDb  { 0.0f };
    std::atomic<float> midDb  { 0.0f };
    std::atomic<float> highDb { 0.0f };
};

} // namespace rollforge
