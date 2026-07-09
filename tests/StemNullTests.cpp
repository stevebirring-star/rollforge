// RollForge — WavExporter / stem tests (Phase 6, commit 4).
//
// Headless: with master FX off, the per-pad stems sum back to the full mix
// (linear); exportStems writes a WAV per active pad.

#include "engine/DrumEngine.h"
#include "engine/WavExporter.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class StemNullTest final : public juce::UnitTest
{
public:
    StemNullTest() : juce::UnitTest ("RollForge StemNull", testCategory) {}

    void runTest() override
    {
        beginTest ("per-pad stems sum to the full mix (master FX off)");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 3; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16; p.lane (0).step (0).on = true; p.lane (0).step (8).on = true;
            p.lane (1).targetPad = 1; p.lane (1).length = 16; p.lane (1).step (4).on = true; p.lane (1).step (12).on = true;
            p.lane (2).targetPad = 2; p.lane (2).length = 16; p.lane (2).step (2).on = true;

            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 0.5;

            juce::AudioBuffer<float> mix;
            OfflineRenderer::render (engine, p, mix, opts);
            const int n = mix.getNumSamples();

            juce::AudioBuffer<float> sum (2, n);
            sum.clear();
            for (int pad = 0; pad < 16; ++pad)
            {
                juce::AudioBuffer<float> stem;
                WavExporter::renderStem (engine, p, pad, stem, opts);
                const int m = juce::jmin (n, stem.getNumSamples());
                for (int ch = 0; ch < 2; ++ch)
                    sum.addFrom (ch, 0, stem, ch, 0, m);
            }

            expect (mix.getMagnitude (0, 0, n) > 0.0f);
            float maxDiff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (sum.getSample (ch, i) - mix.getSample (ch, i)));
            expect (maxDiff < 0.01f);
        }

        beginTest ("stems still sum to the mix once pads feed the reverb send");
        {
            // The per-pad send reverb lives in the DrumEngine, so it runs for stems too.
            // Stems keep summing to the mix ONLY because Space is linear:
            //     reverb(send_a + send_b) == reverb(send_a) + reverb(send_b)
            // Every render calls engine.prepare(), which resets the reverb, so each stem
            // starts from the same state as the mix. Add a saturator to the send and this
            // test is what tells you the WYSIWYG export just broke.
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            kit.pad (0).reverbSend = 0.7f;    // kick, wet
            kit.pad (1).reverbSend = 0.35f;   // snare, half-wet
            kit.pad (2).tone       = -0.6f;   // and a tilted pad, to exercise the tone path
            kit.pad (2).reverbSend = 1.0f;
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 3; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16; p.lane (0).step (0).on = true; p.lane (0).step (8).on = true;
            p.lane (1).targetPad = 1; p.lane (1).length = 16; p.lane (1).step (4).on = true; p.lane (1).step (12).on = true;
            p.lane (2).targetPad = 2; p.lane (2).length = 16; p.lane (2).step (2).on = true;

            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 0.5;

            juce::AudioBuffer<float> dryMix;
            {
                DrumEngine dryEngine;
                Kit dryKit = StarterKit::build (44100.0);   // no sends at all
                installKitIntoEngine (dryKit, dryEngine);
                OfflineRenderer::render (dryEngine, p, dryMix, opts);
            }

            juce::AudioBuffer<float> mix;
            OfflineRenderer::render (engine, p, mix, opts);
            const int n = mix.getNumSamples();

            // The send must actually be doing something, or this test proves nothing.
            float wetDiff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < juce::jmin (n, dryMix.getNumSamples()); ++i)
                    wetDiff = juce::jmax (wetDiff, std::abs (mix.getSample (ch, i) - dryMix.getSample (ch, i)));
            expect (wetDiff > 0.001f, "the per-pad send is audible in the mix");

            juce::AudioBuffer<float> sum (2, n);
            sum.clear();
            for (int pad = 0; pad < 16; ++pad)
            {
                juce::AudioBuffer<float> stem;
                WavExporter::renderStem (engine, p, pad, stem, opts);
                const int m = juce::jmin (n, stem.getNumSamples());
                for (int ch = 0; ch < 2; ++ch)
                    sum.addFrom (ch, 0, stem, ch, 0, m);
            }

            expect (mix.getMagnitude (0, 0, n) > 0.0f);
            float maxDiff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (sum.getSample (ch, i) - mix.getSample (ch, i)));
            expect (maxDiff < 0.01f, "wet stems sum to the wet mix, maxDiff = " + juce::String (maxDiff));
        }

        beginTest ("exportStems writes a WAV per active pad");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 2; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16; p.lane (0).step (0).on = true;
            p.lane (1).targetPad = 1; p.lane (1).length = 16; p.lane (1).step (4).on = true;

            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("rollforge_stems");
            dir.deleteRecursively();

            OfflineRenderer::Options opts;
            opts.bars = 1; opts.tailSeconds = 0.2;

            const int written = WavExporter::exportStems (engine, p, dir, opts);
            expectEquals (written, 2);
            expect (dir.getChildFile ("pad_00.wav").existsAsFile());
            expect (dir.getChildFile ("pad_01.wav").existsAsFile());

            dir.deleteRecursively();
        }
    }
};

static StemNullTest stemNullTest;

} // namespace rollforge::tests
