#include "engine/fx/Space.h"

#include <cmath>

namespace rollforge
{

void Space::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    const double scale = sampleRate / 44100.0;

    const int combTuning[numCombs]   = { 1116, 1188, 1277, 1356 };
    const int apTuning[numAllpass]   = { 556, 441 };

    for (int i = 0; i < numCombs; ++i)
        combs[i].setSize ((int) std::lround (combTuning[i] * scale));
    for (int i = 0; i < numAllpass; ++i)
        allpasses[i].setSize ((int) std::lround (apTuning[i] * scale));

    // Pre-lowpass (~6 kHz) for a darker plate.
    const double x = std::exp (-2.0 * 3.14159265358979 * 6000.0 / sampleRate);
    preLpCoeff = (float) (1.0 - x);
    preLpState = 0.0f;
}

void Space::reset() noexcept
{
    for (auto& c : combs) c.clear();
    for (auto& ap : allpasses) ap.clear();
    preLpState = 0.0f;
}

float Space::reverbSample (float in) noexcept
{
    constexpr float inGain = 0.015f;   // tames the ~4/(1-feedback) comb gain

    preLpState += preLpCoeff * (in - preLpState);
    const float src = preLpState * inGain;

    float wet = 0.0f;
    for (int k = 0; k < numCombs; ++k)
        wet += combs[k].process (src, feedback, damp);
    for (int k = 0; k < numAllpass; ++k)
        wet = allpasses[k].process (wet, 0.5f);

    return wet;
}

void Space::process (juce::AudioBuffer<float>& buffer) noexcept
{
    float a = target.load (std::memory_order_acquire);
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    if (a <= 0.0f)
        return;   // bypass

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    const float invNumCh = 1.0f / (float) numCh;

    for (int i = 0; i < n; ++i)
    {
        float in = 0.0f;
        for (int c = 0; c < numCh; ++c)
            in += buffer.getSample (c, i);
        in *= invNumCh;

        const float wet = reverbSample (in);

        for (int c = 0; c < numCh; ++c)
            buffer.setSample (c, i, buffer.getSample (c, i) + a * wet);   // send blend
    }
}

void Space::processSend (const float* sendIn, juce::AudioBuffer<float>& out, int numSamples) noexcept
{
    const int numCh = out.getNumChannels();
    if (sendIn == nullptr || numCh <= 0 || numSamples <= 0)
        return;

    // No amount, no bypass: the per-pad send levels already scaled sendIn, and a silent
    // input must still be pushed through so the tail from earlier hits keeps decaying.
    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = reverbSample (sendIn[i]);
        for (int c = 0; c < numCh; ++c)
            out.addSample (c, i, wet);
    }
}

} // namespace rollforge
