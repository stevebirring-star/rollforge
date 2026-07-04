#include "engine/WavExporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace rollforge
{
namespace WavExporter
{

namespace
{
    // A copy of the pattern with only the lanes + rolls targeting `pad` active.
    Pattern stemPattern (const Pattern& p, int pad)
    {
        Pattern s = p;   // off-audio-thread copy

        const int lanes = juce::jlimit (0, maxLanes, s.numLanes);
        for (int li = 0; li < lanes; ++li)
            if (s.lane (li).targetPad != pad)
                for (int st = 0; st < maxStepsPerLane; ++st)
                    s.lane (li).step (st).on = false;

        int keep = 0;
        const int rolls = juce::jlimit (0, maxRolls, s.numRolls);
        for (int ri = 0; ri < rolls; ++ri)
            if (s.rolls[(size_t) ri].targetPad == pad)
                s.rolls[(size_t) keep++] = s.rolls[(size_t) ri];
        s.numRolls = keep;

        return s;
    }

    bool padHasContent (const Pattern& p, int pad)
    {
        const int lanes = juce::jlimit (0, maxLanes, p.numLanes);
        for (int li = 0; li < lanes; ++li)
        {
            const Lane& lane = p.lane (li);
            if (lane.targetPad != pad)
                continue;
            const int len = juce::jlimit (1, maxStepsPerLane, lane.length);
            for (int st = 0; st < len; ++st)
                if (lane.step (st).on)
                    return true;
        }
        const int rolls = juce::jlimit (0, maxRolls, p.numRolls);
        for (int ri = 0; ri < rolls; ++ri)
            if (p.rolls[(size_t) ri].targetPad == pad && p.rolls[(size_t) ri].count > 0)
                return true;
        return false;
    }
}

bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file)
{
    file.deleteFile();
    juce::WavAudioFormat fmt;
    auto os = file.createOutputStream();
    if (os == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (os.get(), sampleRate,
                             (unsigned int) juce::jmax (1, buffer.getNumChannels()), 24,
                             juce::StringPairArray(), 0));
    if (writer == nullptr)
        return false;
    os.release();   // the writer owns the stream now

    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

bool exportMix (DrumEngine& engine, const Pattern& pattern, const juce::File& file,
                const OfflineRenderer::Options& opts)
{
    juce::AudioBuffer<float> mix;
    OfflineRenderer::render (engine, pattern, mix, opts);
    return writeWav (mix, opts.sampleRate, file);
}

int renderStem (DrumEngine& engine, const Pattern& pattern, int padIndex,
                juce::AudioBuffer<float>& out, const OfflineRenderer::Options& opts)
{
    return OfflineRenderer::render (engine, stemPattern (pattern, padIndex), out, opts);
}

int exportStems (DrumEngine& engine, const Pattern& pattern, const juce::File& folder,
                 const OfflineRenderer::Options& opts)
{
    folder.createDirectory();
    int written = 0;
    for (int pad = 0; pad < 16; ++pad)
    {
        if (! padHasContent (pattern, pad))
            continue;

        juce::AudioBuffer<float> stem;
        renderStem (engine, pattern, pad, stem, opts);

        const juce::File file = folder.getChildFile ("pad_" + juce::String (pad).paddedLeft ('0', 2) + ".wav");
        if (writeWav (stem, opts.sampleRate, file))
            ++written;
    }
    return written;
}

} // namespace WavExporter
} // namespace rollforge
