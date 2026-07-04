// RollForge — OfflineRenderer tests (Phase 6, commit 3).
//
// Headless: rendering a kit + pattern yields a non-silent stereo buffer of the
// expected length; an empty pattern renders silence.

#include "engine/DrumEngine.h"
#include "engine/OfflineRenderer.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class OfflineRenderTest final : public juce::UnitTest
{
public:
    OfflineRenderTest() : juce::UnitTest ("RollForge OfflineRender", testCategory) {}

    void runTest() override
    {
        beginTest ("renders a pattern to a non-silent stereo buffer of the right length");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            for (int s = 0; s < 16; s += 4)
                p.lane (0).step (s).on = true;

            juce::AudioBuffer<float> out;
            OfflineRenderer::Options opts;
            opts.sampleRate = 44100.0; opts.bars = 1;

            const int n = OfflineRenderer::render (engine, p, out, opts);
            expect (n > 0);
            expectEquals (out.getNumSamples(), n);
            expectEquals (out.getNumChannels(), 2);
            expect (out.getMagnitude (0, 0, n) > 0.0f);
        }

        beginTest ("empty pattern renders (near-)silence");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).length = 16;   // all steps off

            juce::AudioBuffer<float> out;
            OfflineRenderer::Options opts;
            OfflineRenderer::render (engine, p, out, opts);

            expect (out.getMagnitude (0, 0, out.getNumSamples()) < 0.001f);
        }
    }
};

static OfflineRenderTest offlineRenderTest;

} // namespace rollforge::tests
