#include "engine/fx/Crush.h"

#include <cmath>

namespace rollforge
{

void Crush::prepare (double /*sampleRate*/) noexcept
{
    reset();
}

void Crush::reset() noexcept
{
    for (auto& v : held)
        v = 0.0f;
    holdCounter = 0;
}

void Crush::process (juce::AudioBuffer<float>& buffer) noexcept
{
    float a = target.load (std::memory_order_acquire);
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    if (a <= 0.0f)
        return;   // bypass

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    const int   useCh       = numCh < maxCh ? numCh : maxCh;
    const float bits        = 16.0f - a * 12.0f;          // 16 -> 4
    const float levels      = std::pow (2.0f, bits);      // quantisation steps
    const int   holdSamples = 1 + (int) (a * 15.0f);      // downsample factor 1..16

    for (int i = 0; i < n; ++i)
    {
        if (holdCounter <= 0)
        {
            for (int c = 0; c < useCh; ++c)
                held[c] = buffer.getSample (c, i);
            holdCounter = holdSamples;
        }
        --holdCounter;

        for (int c = 0; c < useCh; ++c)
        {
            const float dry = buffer.getSample (c, i);
            const float q   = std::round (held[c] * levels) / levels;   // bit-crushed, downsampled
            const float wet = std::tanh (q);                            // soft clip
            buffer.setSample (c, i, dry + a * (wet - dry));             // dry/wet
        }
    }
}

} // namespace rollforge
