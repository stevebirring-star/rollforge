#include "engine/Resample.h"

#include <cmath>

namespace rollforge
{
namespace Resample
{

int loopSamples (double sampleRate, double bpm, int bars) noexcept
{
    const double sr    = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double tempo = bpm        > 0.0 ? bpm        : 120.0;
    const int    n     = bars       > 0   ? bars       : 1;

    // One bar of 4/4 is four beats, and a beat is 60 / bpm seconds.
    const double seconds = (double) n * 4.0 * 60.0 / tempo;
    const long long total = std::llround (seconds * sr);
    return total < 1 ? 1 : (int) total;
}

float foldTailIntoLoop (juce::AudioBuffer<float>& buffer, int loop)
{
    const int total    = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    if (loop < 1 || channels < 1)
        return 0.0f;

    if (total > loop)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);

            // `i % loop` rather than `i - loop`: a reverb tail can outlast the bar it decays
            // from, and each pass of the loop would have layered it again.
            for (int i = loop; i < total; ++i)
                data[i % loop] += data[i];
        }

        buffer.setSize (channels, loop, /*keepExistingContent*/ true, false, /*avoidReallocating*/ true);
    }

    return buffer.getMagnitude (0, buffer.getNumSamples());
}

float limitPeak (juce::AudioBuffer<float>& buffer, float target)
{
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() <= 0 || target <= 0.0f)
        return 1.0f;

    const float peak = buffer.getMagnitude (0, n);
    if (peak <= target || peak <= 0.0f)
        return 1.0f;

    const float gain = target / peak;
    buffer.applyGain (gain);
    return gain;
}

} // namespace Resample
} // namespace rollforge
