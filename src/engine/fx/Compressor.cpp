#include "engine/fx/Compressor.h"

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

void Compressor::prepare (double sr) noexcept
{
    sampleRate   = sr > 0.0 ? sr : 44100.0;
    attackCoeff  = coeffFor (10.0,  sampleRate);   // ~10 ms attack
    releaseCoeff = coeffFor (120.0, sampleRate);   // ~120 ms release
    reset();
}

void Compressor::reset() noexcept
{
    envelope = 0.0f;
}

void Compressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    float a = target.load (std::memory_order_acquire);
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    if (a <= 0.0f)
        return;   // bypass

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;
    const int useCh = numCh < maxCh ? numCh : maxCh;

    // Amount maps to a musical threshold + ratio, with partial auto makeup so a
    // higher setting sounds glued and a touch louder without runaway level.
    const float thresholdDb = -a * 24.0f;                 // 0 -> -24 dBFS
    const float ratio       = 1.0f + a * 3.0f;            // 1:1 -> 4:1
    const float slope       = 1.0f - 1.0f / ratio;        // gain-reduction slope
    const float makeupDb    = -thresholdDb * slope * 0.4f;
    const float makeup      = std::pow (10.0f, makeupDb / 20.0f);

    float* chans[maxCh];
    for (int c = 0; c < useCh; ++c)
        chans[c] = buffer.getWritePointer (c);

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < useCh; ++c)
        {
            const float v = std::abs (chans[c][i]);
            if (v > peak) peak = v;
        }

        // Attack when rising, release when falling (linked across channels).
        if (peak > envelope) envelope = attackCoeff  * envelope + (1.0f - attackCoeff)  * peak;
        else                 envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * peak;

        const float levelDb = 20.0f * std::log10 (envelope + 1.0e-9f);
        const float overDb  = levelDb - thresholdDb;
        const float grDb    = overDb > 0.0f ? overDb * slope : 0.0f;
        const float gain    = makeup * std::pow (10.0f, -grDb / 20.0f);

        for (int c = 0; c < useCh; ++c)
            chans[c][i] *= gain;
    }
}

} // namespace rollforge
