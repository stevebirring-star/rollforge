// RollForge — StarterKit + Kit-install unit tests.
//
// Headless, end-to-end through the real path: build the synthesised kit, install
// it into a DrumEngine via the command FIFO, then trigger pads and check that
// pads produce sound, different pads sound different, the two hats choke each
// other, and non-choke pads stay polyphonic.

#include "engine/DrumEngine.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    bool isSilent (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            if (buffer.getMagnitude (ch, 0, buffer.getNumSamples()) > 0.0f)
                return false;
        return true;
    }
}

class StarterKitTest final : public juce::UnitTest
{
public:
    StarterKitTest() : juce::UnitTest ("RollForge StarterKit", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("builds 16 pads, all with samples; hats share a choke group");
        {
            auto kit = StarterKit::build (sr);
            expectEquals ((int) kit.pads.size(), 16);
            for (int i = 0; i < 16; ++i)
                expect (kit.pad (i).hasSample());

            expect (kit.pad (StarterKit::Kick).primarySample() != kit.pad (StarterKit::Snare).primarySample());
            expect (kit.pad (StarterKit::ClosedHat).primarySample() != kit.pad (StarterKit::OpenHat).primarySample());
            expectEquals (kit.pad (StarterKit::ClosedHat).chokeGroup, StarterKit::hatChokeGroup);
            expectEquals (kit.pad (StarterKit::OpenHat).chokeGroup, StarterKit::hatChokeGroup);
        }

        beginTest ("installed kit -> triggering a pad produces sound");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            auto kit = StarterKit::build (sr);
            installKitIntoEngine (kit, engine);

            engine.pushTrigger (StarterKit::Kick, 1.0f);
            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            engine.process (buf);          // drains 16 setPad + trigger, renders

            expect (! isSilent (buf));
        }

        beginTest ("different pads play different sounds");
        {
            auto energyOf = [&] (int padIndex)
            {
                DrumEngine engine;
                engine.prepare (sr, 512);
                auto kit = StarterKit::build (sr);
                installKitIntoEngine (kit, engine);
                engine.pushTrigger (padIndex, 1.0f);
                juce::AudioBuffer<float> buf (1, 512);
                buf.clear();
                engine.process (buf);

                double sum = 0.0;
                const auto* d = buf.getReadPointer (0);
                for (int i = 0; i < 512; ++i)
                    sum += std::abs (d[i]);
                return sum;
            };

            const double kick = energyOf (StarterKit::Kick);
            const double hat  = energyOf (StarterKit::ClosedHat);
            expect (kick > 0.0);
            expect (hat > 0.0);
            expect (std::abs (kick - hat) > 1.0e-2);   // clearly different signals
        }

        beginTest ("closed hat chokes open hat (end to end)");
        {
            DrumEngine engine;
            engine.prepare (sr, 128);
            auto kit = StarterKit::build (sr);
            installKitIntoEngine (kit, engine);
            juce::AudioBuffer<float> buf (2, 128);

            engine.pushTrigger (StarterKit::OpenHat, 1.0f);
            buf.clear();
            engine.process (buf);          // applies setPads + open hat
            expectEquals (engine.getNumActiveVoices(), 1);

            engine.pushTrigger (StarterKit::ClosedHat, 1.0f);
            buf.clear();
            engine.process (buf);          // closed hat chokes the open hat
            expectEquals (engine.getNumActiveVoices(), 1);
        }

        beginTest ("non-choke pads stay polyphonic");
        {
            DrumEngine engine;
            engine.prepare (sr, 128);
            auto kit = StarterKit::build (sr);
            installKitIntoEngine (kit, engine);
            juce::AudioBuffer<float> buf (2, 128);

            engine.pushTrigger (StarterKit::Kick, 1.0f);
            engine.pushTrigger (StarterKit::Snare, 1.0f);
            buf.clear();
            engine.process (buf);
            expectEquals (engine.getNumActiveVoices(), 2);
        }
    }
};

static StarterKitTest starterKitTest;

} // namespace rollforge::tests
