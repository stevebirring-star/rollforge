#pragma once

// RollForge — OutputMeter: what a master VU needle and its peak lamp should read.
//
// Two different measurements, because they answer two different questions:
//
//   * VU  — a true analogue VU meter is a MEAN meter with 300 ms ballistics: fed a steady
//           tone it reaches 99% of the reading in 300 ms, and falls back at the same rate.
//           It deliberately ignores transients, which is what makes it read as "loudness"
//           rather than as "samples". On a drum machine that is exactly the useful thing:
//           a kick that pins a peak meter barely moves a VU.
//   * PEAK — a sample-peak meter with an instant attack and a slow release, for the one
//           question a VU cannot answer: am I about to clip?
//
// 0 VU is referenced to -18 dBFS here (the usual digital alignment for +4 dBu), so a
// well-levelled mix sits around the 0 mark with headroom above it.
//
// Pure: no JUCE, no allocation, no locks. process() is called once per audio block from
// the audio thread; the readings are published through atomics for the UI to sample.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <atomic>
#include <cmath>

namespace rollforge
{

class OutputMeter
{
public:
    /** dBFS that reads as 0 VU. -18 dBFS is the common alignment for +4 dBu. */
    static constexpr float zeroVuDbfs = -18.0f;

    /** Integration time of a true VU meter: 99% of a step in 300 ms, and the same back. */
    static constexpr float vuIntegrationSeconds = 0.3f;

    /** How long a peak reading takes to fall away once the signal stops. */
    static constexpr float peakReleaseSeconds = 1.6f;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            vuLinear[c] = 0.0f;
            peakLinear[c].store (0.0f, std::memory_order_relaxed);
            vu[c].store (0.0f, std::memory_order_relaxed);
        }
    }

    /** Feeds one block of one channel. Audio thread; allocation- and lock-free.
        `channel` past 1 is ignored, so a mono or 5.1 device can't write out of bounds. */
    void processBlock (int channel, const float* samples, int numSamples) noexcept
    {
        if (channel < 0 || channel > 1 || samples == nullptr || numSamples <= 0)
            return;

        double sumSquares = 0.0;
        float  blockPeak  = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = samples[i];
            sumSquares += (double) s * (double) s;
            const float a = std::fabs (s);
            if (a > blockPeak)
                blockPeak = a;
        }
        const float blockRms = (float) std::sqrt (sumSquares / (double) numSamples);

        // 99% of a step in `vuIntegrationSeconds` -> a one-pole whose coefficient is
        // derived per BLOCK, so the reading is independent of the host's buffer size.
        // ln(1 - 0.99) = -4.6, hence the 4.6 / T time constant.
        const float blockSeconds = (float) ((double) numSamples / sr);
        const float vuCoeff = 1.0f - std::exp (-4.6f * blockSeconds / vuIntegrationSeconds);
        vuLinear[channel] += (blockRms - vuLinear[channel]) * vuCoeff;
        vu[channel].store (vuLinear[channel], std::memory_order_relaxed);

        // Peak: instant attack, exponential release. It must never miss a transient, so
        // it takes the block peak directly rather than any smoothed value.
        const float peakCoeff = std::exp (-blockSeconds / peakReleaseSeconds);
        float held = peakLinear[channel].load (std::memory_order_relaxed) * peakCoeff;
        if (blockPeak > held)
            held = blockPeak;
        peakLinear[channel].store (held, std::memory_order_relaxed);
    }

    /** VU reading in dB relative to 0 VU (so 0 == the "0 VU" mark, +3 is the red zone).
        Message thread; a benign relaxed read, exactly like the per-pad meters. */
    float getVuDb (int channel) const noexcept
    {
        if (channel < 0 || channel > 1)
            return -60.0f;
        return toDbfs (vu[channel].load (std::memory_order_relaxed)) - zeroVuDbfs;
    }

    /** Sample-peak reading in dBFS (0 == full scale). Message thread. */
    float getPeakDbfs (int channel) const noexcept
    {
        if (channel < 0 || channel > 1)
            return -60.0f;
        return toDbfs (peakLinear[channel].load (std::memory_order_relaxed));
    }

    /** Linear 0..1 peak, for a simple lamp. Message thread. */
    float getPeakLinear (int channel) const noexcept
    {
        return (channel >= 0 && channel <= 1)
                   ? peakLinear[channel].load (std::memory_order_relaxed) : 0.0f;
    }

    static float toDbfs (float linear) noexcept
    {
        return linear > 1.0e-6f ? 20.0f * std::log10 (linear) : -120.0f;
    }

private:
    double sr = 44100.0;

    // vuLinear is audio-thread-only state; vu[] and peakLinear[] are published.
    float                           vuLinear[2] { 0.0f, 0.0f };
    std::atomic<float>              vu[2] {};
    std::atomic<float>              peakLinear[2] {};
};

} // namespace rollforge
