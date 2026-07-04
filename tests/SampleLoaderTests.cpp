// RollForge — SampleLoader unit tests.
//
// Headless: encode a known signal to an in-memory WAV, decode it back through
// the loader, and check channels / rate / length / content survive the round
// trip at the file's native rate. Also checks format reporting and rejection of
// non-audio data.

#include "library/SampleLoader.h"

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    // Encodes `source` to a 16-bit WAV in memory and returns the bytes.
    juce::MemoryBlock encodeWav (const juce::AudioBuffer<float>& source, double sampleRate)
    {
        juce::MemoryBlock block;
        juce::WavAudioFormat wav;
        auto* out = new juce::MemoryOutputStream (block, false);

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (out, sampleRate, (unsigned int) source.getNumChannels(), 16, {}, 0));

        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples());
        else
            delete out;   // createWriterFor did not take ownership on failure

        return block;   // writer's destructor flushed + deleted `out`
    }
}

class SampleLoaderTest final : public juce::UnitTest
{
public:
    SampleLoaderTest() : juce::UnitTest ("RollForge SampleLoader", testCategory) {}

    void runTest() override
    {
        beginTest ("decodes an in-memory WAV round-trip at native rate");
        {
            const int    n        = 128;
            const int    channels = 2;
            const double rate     = 48000.0;

            juce::AudioBuffer<float> original (channels, n);
            for (int i = 0; i < n; ++i)
            {
                original.setSample (0, i, std::sin ((float) i * 0.20f) * 0.5f);
                original.setSample (1, i, std::cos ((float) i * 0.15f) * 0.4f);   // distinct channel
            }

            const auto wavBytes = encodeWav (original, rate);
            expect (wavBytes.getSize() > 44);   // at least a WAV header

            SampleLoader loader;
            auto buf = loader.loadFromMemory (wavBytes.getData(), wavBytes.getSize(), "test.wav");
            expect (buf != nullptr);

            if (buf != nullptr)
            {
                expectEquals (buf->getNumChannels(), channels);
                expectEquals (buf->getNumSamples(), n);
                expectWithinAbsoluteError (buf->getSampleRate(), rate, 1.0e-6);

                // 16-bit quantisation -> compare within ~1/32768.
                for (int i = 0; i < n; i += 16)
                {
                    expectWithinAbsoluteError (buf->getSample (0, i), original.getSample (0, i), 1.0e-3f);
                    expectWithinAbsoluteError (buf->getSample (1, i), original.getSample (1, i), 1.0e-3f);
                }
            }
        }

        beginTest ("preserves a non-standard sample rate");
        {
            const int n = 64;
            juce::AudioBuffer<float> original (1, n);
            for (int i = 0; i < n; ++i)
                original.setSample (0, i, (float) (i % 8) / 8.0f);

            const auto wavBytes = encodeWav (original, 22050.0);
            SampleLoader loader;
            auto buf = loader.loadFromMemory (wavBytes.getData(), wavBytes.getSize(), "lofi.wav");

            expect (buf != nullptr);
            if (buf != nullptr)
            {
                expectEquals (buf->getNumChannels(), 1);
                expectWithinAbsoluteError (buf->getSampleRate(), 22050.0, 1.0e-6);
            }
        }

        beginTest ("reports common formats");
        {
            SampleLoader loader;
            const auto wildcards = loader.getSupportedWildcards();
            expect (wildcards.containsIgnoreCase ("wav"));
            expect (wildcards.containsIgnoreCase ("aif"));
        }

        beginTest ("rejects non-audio data");
        {
            const char junk[] = "this is definitely not an audio file, just some text";
            SampleLoader loader;
            auto buf = loader.loadFromMemory (junk, sizeof (junk), "junk.bin");
            expect (buf == nullptr);
        }

        beginTest ("rejects an empty/missing input");
        {
            SampleLoader loader;
            expect (loader.loadFromMemory (nullptr, 0, "none") == nullptr);
            expect (loader.loadFile (juce::File()) == nullptr);
        }
    }
};

static SampleLoaderTest sampleLoaderTest;

} // namespace rollforge::tests
