// RollForge — KitInstaller unit tests.
//
// Headless checks of the model->engine bridge: Pad->VoiceParameters mapping, and
// the live pad-sample swap (installSampleIntoPad) that exercises the retirement
// pool for the first time end-to-end — the old buffer is retired (not freed on
// the audio thread) and reclaimed on the message thread once nothing references it.

#include "engine/DrumEngine.h"
#include "engine/SampleRetirementPool.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    SampleBuffer::Ptr makeSample (float value, int n = 16)
    {
        juce::AudioBuffer<float> audio (1, n);
        for (int i = 0; i < n; ++i)
            audio.setSample (0, i, value);
        return new SampleBuffer (std::move (audio), 44100.0, "x");
    }

    bool isSilent (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            if (buffer.getMagnitude (ch, 0, buffer.getNumSamples()) > 0.0f)
                return false;
        return true;
    }
}

class KitInstallerTest final : public juce::UnitTest
{
public:
    KitInstallerTest() : juce::UnitTest ("RollForge KitInstaller", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("toVoiceParameters maps every Pad field");
        {
            Pad pad;
            pad.volume = 0.7f; pad.pan = -0.5f; pad.pitchSemis = 3.0f;
            pad.attackMs = 2.0f; pad.releaseMs = 5.0f; pad.reverse = true;

            const auto vp = toVoiceParameters (pad);
            expectWithinAbsoluteError (vp.gain, 0.7f, 1.0e-6f);
            expectWithinAbsoluteError (vp.pan, -0.5f, 1.0e-6f);
            expectWithinAbsoluteError (vp.pitchSemitones, 3.0f, 1.0e-6f);
            expectWithinAbsoluteError (vp.attackMs, 2.0f, 1.0e-6f);
            expectWithinAbsoluteError (vp.releaseMs, 5.0f, 1.0e-6f);
            expect (vp.reverse);
        }

        beginTest ("installSampleIntoPad swaps the sample and reclaims the old one");
        {
            DrumEngine engine;
            engine.prepare (sr, 64);
            SampleRetirementPool pool;
            Kit kit = StarterKit::build (sr);
            installKitIntoEngine (kit, engine);

            juce::AudioBuffer<float> buf (2, 64);
            buf.clear();
            engine.process (buf);                    // apply the 16 initial setPads

            auto oldSample = kit.pad (0).primarySample();
            expect (oldSample != nullptr);
            auto newSample = makeSample (0.5f);

            installSampleIntoPad (pool, kit, engine, 0, newSample);
            expect (kit.pad (0).primarySample() == newSample);
            expectEquals (pool.getNumRetired(), 1);

            buf.clear();
            engine.process (buf);                    // engine adopts new sample, drops old
            engine.pushTrigger (0, 1.0f);
            buf.clear();
            engine.process (buf);
            expect (! isSilent (buf));               // now plays the new sample

            // Old buffer is held by the pool + our local ref -> sweep keeps it.
            expectEquals (pool.sweep(), 0);
            oldSample = nullptr;                     // drop our ref -> only the pool holds it
            expectEquals (pool.sweep(), 1);          // reclaimed (message thread)
            expectEquals (pool.getNumRetired(), 0);
        }

        beginTest ("installSampleIntoPad ignores invalid input");
        {
            DrumEngine engine;
            engine.prepare (sr, 64);
            SampleRetirementPool pool;
            Kit kit = StarterKit::build (sr);

            installSampleIntoPad (pool, kit, engine, -1, makeSample (0.5f));
            installSampleIntoPad (pool, kit, engine, 99, makeSample (0.5f));
            installSampleIntoPad (pool, kit, engine, 0, nullptr);
            expectEquals (pool.getNumRetired(), 0);
        }
    }
};

static KitInstallerTest kitInstallerTest;

} // namespace rollforge::tests
