#include "library/SampleAnalyser.h"

#include "library/Categoriser.h"
#include "library/FeatureExtractor.h"

#include <vector>

namespace rollforge
{

bool SampleAnalyser::analyse (const juce::File& file, LibraryEntry& out)
{
    const SampleBuffer::Ptr sample = loader.loadFile (file);
    if (sample == nullptr || sample->getNumSamples() <= 0)
        return false;

    const int n  = sample->getNumSamples();
    const int ch = sample->getNumChannels();

    // Mono-sum for analysis.
    std::vector<float> mono ((std::size_t) n, 0.0f);
    const juce::AudioBuffer<float>& buf = sample->getAudio();
    for (int c = 0; c < ch; ++c)
    {
        const float* p = buf.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            mono[(std::size_t) i] += p[i];
    }
    if (ch > 1)
    {
        const float inv = 1.0f / (float) ch;
        for (auto& v : mono) v *= inv;
    }

    const AudioFeatures features = FeatureExtractor::analyse (mono.data(), n, sample->getSampleRate());

    out.path            = file.getFullPathName();
    out.name            = file.getFileNameWithoutExtension();
    out.durationSeconds = features.durationSeconds;
    out.rms             = features.rms;
    out.zcr             = features.zcr;
    out.decay           = features.decay;
    out.onsetCount      = features.onsetCount;
    out.category        = Categoriser::categorise (file.getFileName(), features);
    out.confidence      = 1.0f;
    out.favourite       = false;
    return true;
}

} // namespace rollforge
