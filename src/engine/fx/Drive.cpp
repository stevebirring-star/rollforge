#include "engine/fx/Drive.h"

#include <cmath>

namespace rollforge
{

void Drive::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;

    // One-pole lowpass (~3 kHz) whose "removed highs" drive the shelf brighten.
    const double fc = 3000.0;
    const double x  = std::exp (-2.0 * 3.14159265358979 * fc / sampleRate);
    lpCoeff = (float) (1.0 - x);
    reset();
}

void Drive::reset() noexcept
{
    for (auto& v : lp)
        v = 0.0f;
}

void Drive::process (juce::AudioBuffer<float>& buffer) noexcept
{
    float a = target.load (std::memory_order_acquire);
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    if (a <= 0.0f)
        return;   // bypass

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    const int   useCh     = numCh < maxCh ? numCh : maxCh;
    const float driveGain = 1.0f + a * 8.0f;   // 1..9
    const float shelfAmt  = a * 0.4f;

    for (int c = 0; c < useCh; ++c)
    {
        float* d   = buffer.getWritePointer (c);
        float  lpc = lp[c];

        for (int i = 0; i < n; ++i)
        {
            const float dry = d[i];
            float wet = std::tanh (dry * driveGain);   // soft saturation
            lpc += lpCoeff * (wet - lpc);              // track lows
            wet  = wet + shelfAmt * (wet - lpc);       // add the highs back louder
            d[i] = dry + a * (wet - dry);              // dry/wet by amount
        }

        lp[c] = lpc;
    }
}

} // namespace rollforge
