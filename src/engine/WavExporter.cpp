#include "engine/WavExporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace rollforge
{
namespace WavExporter
{

namespace
{
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
    // Render the WHOLE pattern and capture one pad. Muting the other pads' lanes instead
    // would stop them choking this one: the closed hat would never fire in the open hat's
    // stem, so the open hat would ring on past the point where the mix cuts it dead --
    // and FillEngine places open hats precisely so they get choked.
    OfflineRenderer::Options stemOpts = opts;
    stemOpts.capturePad = padIndex;
    return OfflineRenderer::render (engine, pattern, out, stemOpts);
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
