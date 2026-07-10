#include "engine/fx/MasterLimiter.h"

#include <cmath>

namespace rollforge
{

namespace
{
    // One-pole smoothing coefficient for a time constant in milliseconds.
    float coeffFor (double ms, double sampleRate) noexcept
    {
        const double t = ms * 0.001 * sampleRate;
        return t > 0.0 ? (float) std::exp (-1.0 / t) : 0.0f;
    }
}

void MasterLimiter::prepare (double sr) noexcept
{
    sampleRate   = sr > 0.0 ? sr : 44100.0;
    releaseCoeff = coeffFor (100.0, sampleRate);   // ~100 ms release
    reset();
}

void MasterLimiter::reset() noexcept
{
    envelope = 0.0f;
}

void MasterLimiter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    constexpr int maxCh = 8;
    const int useCh = numCh < maxCh ? numCh : maxCh;
    float* chans[maxCh];
    for (int c = 0; c < useCh; ++c)
        chans[c] = buffer.getWritePointer (c);

    float blockInputPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < useCh; ++c)
        {
            const float a = std::abs (chans[c][i]);
            if (a > peak) peak = a;
        }

        if (peak > blockInputPeak)
            blockInputPeak = peak;

        // Instant attack (catch the transient), smoothed release.
        if (peak > envelope) envelope = peak;
        else                 envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * peak;

        const float gain = envelope > ceiling ? ceiling / envelope : 1.0f;

        for (int c = 0; c < useCh; ++c)
        {
            float v = chans[c][i] * gain;
            v = v >  ceiling ?  ceiling : v;   // brickwall safety clamp
            v = v < -ceiling ? -ceiling : v;
            chans[c][i] = v;
        }
    }

    // Publish the loudest thing we were given, not the loudest thing we let out. The reader
    // zeroes it, so a peak survives exactly until someone has looked at it.
    float previous = inputPeak.load (std::memory_order_acquire);
    while (blockInputPeak > previous
           && ! inputPeak.compare_exchange_weak (previous, blockInputPeak,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire))
    {
    }
}

} // namespace rollforge
