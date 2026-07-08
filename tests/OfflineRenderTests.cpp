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

        beginTest ("render length scales with bar count and tempo (the export-speed contract)");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;

            OfflineRenderer::Options opts;
            opts.sampleRate  = 44100.0;
            opts.tailSeconds = 0.0;   // isolate the pattern length from the decay tail

            juce::AudioBuffer<float> oneBar, twoBars;
            opts.bars = 1;
            const int n1 = OfflineRenderer::render (engine, p, oneBar, opts);
            opts.bars = 2;
            const int n2 = OfflineRenderer::render (engine, p, twoBars, opts);
            expectEquals (n2, n1 * 2);        // twice the bars -> twice the samples

            // Tempo drives length: at 60 BPM a bar is twice as long as at 120. If an
            // export ignored pattern.bpm (the desync bug) it would render the wrong
            // length and the file would play back at the wrong speed.
            juce::AudioBuffer<float> slow;
            Pattern half = p; half.bpm = 60.0;
            opts.bars = 1;
            const int nSlow = OfflineRenderer::render (engine, half, slow, opts);
            expectEquals (nSlow, n1 * 2);
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
