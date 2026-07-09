#pragma once

// RollForge — Space: a short plate-ish reverb (4 parallel comb filters + 2 series
// allpass filters, with a pre-lowpassed input for a darker plate), mixed in as a
// master send by a single 0..1 amount (0 = bypass). One of the four Phase-4
// macros. Hand-rolled (no juce::dsp) -> headless-testable. RT-safe (delay lines
// are sized in prepare(), never in process()).
//
// Two entry points, two roles:
//   * process()     — the master SPACE macro: reads the mix, blends its own wet back in.
//   * processSend() — a real send bus: reads a separate mono send buffer (already scaled
//                     by each pad's send level) and ADDS the wet to the output. It never
//                     early-outs on a silent input, because the tail has to keep ringing
//                     after the last hit that fed it.
//
// Both are strictly linear (combs + allpasses are LTI, no saturation), which is what
// keeps per-pad stems summing to the mix: reverb(a + b) == reverb(a) + reverb(b). Adding
// any nonlinearity here would silently break that guarantee — see StemNullTests.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <vector>

namespace rollforge
{

class Space
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void  setAmount (float amount) noexcept { target.store (amount, std::memory_order_release); }
    float getAmount() const noexcept { return target.load (std::memory_order_acquire); }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Send-bus form: reverberates `numSamples` of the mono `sendIn` and ADDS the wet
        signal to every channel of `out`. Ignores getAmount() — the per-pad send levels
        already scaled the input. Runs unconditionally so a decaying tail survives the
        silence after the last hit. */
    void processSend (const float* sendIn, juce::AudioBuffer<float>& out, int numSamples) noexcept;

private:
    float reverbSample (float in) noexcept;

    struct Comb
    {
        std::vector<float> buf;
        int   idx   = 0;
        float store = 0.0f;

        void setSize (int n) { buf.assign ((size_t) (n < 1 ? 1 : n), 0.0f); idx = 0; store = 0.0f; }
        void clear() { for (auto& v : buf) v = 0.0f; idx = 0; store = 0.0f; }

        float process (float in, float feedback, float damp) noexcept
        {
            const float y = buf[(size_t) idx];
            store = y * (1.0f - damp) + store * damp;
            buf[(size_t) idx] = in + store * feedback;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int idx = 0;

        void setSize (int n) { buf.assign ((size_t) (n < 1 ? 1 : n), 0.0f); idx = 0; }
        void clear() { for (auto& v : buf) v = 0.0f; idx = 0; }

        float process (float in, float feedback) noexcept
        {
            const float y   = buf[(size_t) idx];
            const float out = -in + y;
            buf[(size_t) idx] = in + y * feedback;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };

    static constexpr int numCombs   = 4;
    static constexpr int numAllpass = 2;

    Comb    combs[numCombs];
    Allpass allpasses[numAllpass];
    float   preLpCoeff = 0.0f, preLpState = 0.0f;
    float   feedback = 0.84f, damp = 0.2f;
    double  sampleRate = 44100.0;
    std::atomic<float> target { 0.0f };
};

} // namespace rollforge
