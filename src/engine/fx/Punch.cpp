#include "engine/fx/Punch.h"

#include <cmath>

namespace rollforge
{

namespace
{
    float coeffFor (double ms, double sampleRate) noexcept
    {
        const double t = ms * 0.001 * sampleRate;
        return t > 0.0 ? (float) std::exp (-1.0 / t) : 0.0f;
    }

    float follow (float env, float rectified, float attCoeff, float relCoeff) noexcept
    {
        const float c = rectified > env ? attCoeff : relCoeff;
        return c * env + (1.0f - c) * rectified;
    }
}

void Punch::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    fastAtt = coeffFor (0.5, sampleRate);    // fast follower catches attacks
    fastRel = coeffFor (20.0, sampleRate);
    slowAtt = coeffFor (25.0, sampleRate);   // slow follower lags behind them
    slowRel = coeffFor (150.0, sampleRate);
    reset();
}

void Punch::reset() noexcept
{
    for (auto& v : envFast) v = 0.0f;
    for (auto& v : envSlow) v = 0.0f;
}

void Punch::process (juce::AudioBuffer<float>& buffer) noexcept
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

    for (int c = 0; c < useCh; ++c)
    {
        float* d  = buffer.getWritePointer (c);
        float  ef = envFast[c];
        float  es = envSlow[c];

        for (int i = 0; i < n; ++i)
        {
            const float dry  = d[i];
            const float rect = std::abs (dry);
            ef = follow (ef, rect, fastAtt, fastRel);
            es = follow (es, rect, slowAtt, slowRel);

            const float trans = ef - es;                         // >0 during attacks
            float gain = 1.0f + a * 3.0f * (trans > 0.0f ? trans : 0.0f);
            if (gain > 4.0f) gain = 4.0f;                        // keep it sane

            const float body = std::tanh (dry * 2.0f);           // parallel saturation body
            float wet = dry * gain;
            wet = wet * (1.0f - 0.25f * a) + body * (0.25f * a);

            d[i] = dry + a * (wet - dry);                        // dry/wet
        }

        envFast[c] = ef;
        envSlow[c] = es;
    }
}

} // namespace rollforge
