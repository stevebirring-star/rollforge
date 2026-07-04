#include "engine/SampleBuffer.h"

namespace rollforge
{

SampleBuffer::SampleBuffer (juce::AudioBuffer<float>&& audioIn,
                            double sampleRateIn,
                            juce::String nameIn)
    : audio (std::move (audioIn)),
      sampleRate (sampleRateIn),
      name (std::move (nameIn))
{
    // A zero/negative rate would make every consumer's resample ratio invalid.
    jassert (sampleRate > 0.0);
}

SampleBuffer::~SampleBuffer() = default;

float SampleBuffer::getSample (int channel, int sampleIndex) const noexcept
{
    const int numChannels = audio.getNumChannels();
    if (numChannels == 0 || sampleIndex < 0 || sampleIndex >= audio.getNumSamples())
        return 0.0f;

    return audio.getSample (juce::jlimit (0, numChannels - 1, channel), sampleIndex);
}

} // namespace rollforge
