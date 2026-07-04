#include "library/Scanner.h"

#include "library/Categoriser.h"
#include "library/FeatureExtractor.h"

#include <vector>

namespace rollforge
{

Scanner::Scanner (LibraryDb& dbToUse) : db (dbToUse) {}

bool Scanner::analyseFile (const juce::File& file, LibraryEntry& out)
{
    const SampleBuffer::Ptr sample = loader.loadFile (file);
    if (sample == nullptr || sample->getNumSamples() <= 0)
        return false;

    const int n  = sample->getNumSamples();
    const int ch = sample->getNumChannels();

    // Mono-sum for analysis.
    std::vector<float> mono ((size_t) n, 0.0f);
    const juce::AudioBuffer<float>& buf = sample->getAudio();
    for (int c = 0; c < ch; ++c)
    {
        const float* p = buf.getReadPointer (c);
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] += p[i];
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

int Scanner::scanBlocking (const juce::File& folder)
{
    scanned.store (0, std::memory_order_release);
    if (! folder.isDirectory())
        return 0;

    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, true, loader.getSupportedWildcards());

    int stored = 0;
    for (const auto& f : files)
    {
        LibraryEntry e;
        if (analyseFile (f, e) && db.upsert (e))
            ++stored;
        scanned.fetch_add (1, std::memory_order_acq_rel);
    }
    return stored;
}

} // namespace rollforge
