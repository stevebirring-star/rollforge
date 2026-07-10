// RollForge — WavExporter / stem tests (Phase 6, commit 4).
//
// Headless: with master FX off, the per-pad stems sum back to the full mix
// (linear); exportStems writes a WAV per active pad.

#include "engine/DrumEngine.h"
#include "engine/WavExporter.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <memory>

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

        beginTest ("stems sum to the mix when a choke group fires");
        {
            // The bug this test exists for: a stem used to be rendered from a pattern with
            // every OTHER pad's lanes stripped out, so nothing was left to choke the target
            // pad. The open hat rang out to its full length in its own stem while the mix
            // cut it short -- and FillEngine places open hats specifically to be choked, so
            // this hit real exports, not just contrived ones.
            //
            // StarterKit puts ClosedHat and OpenHat in hatChokeGroup. A 1/16 step at 120 BPM
            // is 125 ms and the open hat rings for 350 ms, so a closed hat two steps later
            // lands squarely in its tail.
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);
            expectEquals (kit.pad (StarterKit::OpenHat).chokeGroup, StarterKit::hatChokeGroup);
            expectEquals (kit.pad (StarterKit::ClosedHat).chokeGroup, StarterKit::hatChokeGroup);

            Pattern p;
            p.numLanes = 2; p.bpm = 120.0;
            p.lane (0).targetPad = StarterKit::OpenHat;
            p.lane (0).length = 16; p.lane (0).step (0).on = true;
            p.lane (1).targetPad = StarterKit::ClosedHat;
            p.lane (1).length = 16; p.lane (1).step (2).on = true;

            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 0.5;

            juce::AudioBuffer<float> mix;
            OfflineRenderer::render (engine, p, mix, opts);
            const int n = mix.getNumSamples();

            // The choke must actually fire in the mix, or this test proves nothing. The
            // closed hat's own sample is over by 305 ms (it starts at 250 ms and is 50 ms
            // long), while an UNCHOKED open hat rings to 350 ms -- so that window holds the
            // tail, and nothing else. Render the open hat alone to see the tail it would
            // have had.
            {
                Pattern openOnly = p;
                openOnly.numLanes = 1;   // drop the closed-hat lane

                juce::AudioBuffer<float> unchoked;
                OfflineRenderer::render (engine, openOnly, unchoked, opts);

                const int from = (int) (0.305 * 44100.0);
                const int to   = (int) (0.350 * 44100.0);
                const float tailUnchoked = unchoked.getMagnitude (0, from, to - from);
                const float tailChoked   = mix.getMagnitude (0, from, to - from);

                expect (tailUnchoked > 0.001f,
                        "the open hat rings past 305 ms when nothing chokes it, mag = "
                            + juce::String (tailUnchoked));
                expect (tailChoked < tailUnchoked * 0.2f,
                        "the closed hat chokes the open hat in the mix, choked = "
                            + juce::String (tailChoked) + " unchoked = " + juce::String (tailUnchoked));
            }

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
            // Rendering every pad and capturing one is EXACT, not approximate: the only
            // difference from the mix is the order the voices are summed in. Hold the
            // tolerance far below the 0.01f the older tests settle for, so that a future
            // change which merely gets close cannot pass.
            expect (maxDiff < 0.00001f, "choked stems sum to the mix, maxDiff = " + juce::String (maxDiff));
        }

        beginTest ("stems sum to the mix under choke, rolls and voice stealing at once");
        {
            // Everything that makes a stem diverge from the mix, in one pattern: chokes
            // that only fire when another pad plays, a roll (FillEngine's open hats are
            // rolled AND choked), reverb sends, and a voice count past the 64-voice pool
            // so that stealing has to happen.
            Kit kit = StarterKit::build (44100.0);
            kit.pad (StarterKit::Kick).reverbSend    = 0.3f;
            kit.pad (StarterKit::OpenHat).reverbSend = 0.6f;

            // A deliberately shallow pool, so 16 pads firing together MUST steal. Stealing
            // picks the quietest-then-oldest voice, so it depends on every pad's envelope
            // -- a stem that only played its own pad would steal completely differently.
            DrumEngine engine (1024, 8, 16);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.bpm = 120.0;
            p.numLanes = 16;
            for (int lane = 0; lane < 16; ++lane)
            {
                p.lane (lane).targetPad = lane;   // StarterKit is one sound per pad
                p.lane (lane).length    = 16;
                for (int st = 0; st < 16; ++st)
                    p.lane (lane).step (st).on = true;
            }

            // A rolled open hat, landing under the closed hat that chokes it.
            p.numRolls = 1;
            CompiledRoll& roll = p.rolls[0];
            roll.targetPad = StarterKit::OpenHat;
            roll.startStep = 4.0f;
            roll.count     = 8;
            for (int i = 0; i < roll.count; ++i)
            {
                roll.events[(size_t) i].stepOffset = (float) i * 0.25f;
                roll.events[(size_t) i].velocity   = 0.9f;
            }

            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 1.0;

            juce::AudioBuffer<float> mix;
            OfflineRenderer::render (engine, p, mix, opts);
            const int n = mix.getNumSamples();
            expect (mix.getMagnitude (0, 0, n) > 0.0f);

            // Prove the pool really is overflowing: the same pattern through a pool deep
            // enough never to steal must come out DIFFERENT. Without this, the test could
            // silently stop exercising voice-stealing and no one would notice.
            {
                DrumEngine deep (1024, 512, 16);
                installKitIntoEngine (kit, deep);

                juce::AudioBuffer<float> deepMix;
                OfflineRenderer::render (deep, p, deepMix, opts);

                float stealDiff = 0.0f;
                const int m = juce::jmin (n, deepMix.getNumSamples());
                for (int i = 0; i < m; ++i)
                    stealDiff = juce::jmax (stealDiff, std::abs (deepMix.getSample (0, i) - mix.getSample (0, i)));
                expect (stealDiff > 0.001f,
                        "the 64-voice pool steals voices on this pattern, diff = " + juce::String (stealDiff));
            }

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

            float maxDiff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (sum.getSample (ch, i) - mix.getSample (ch, i)));
            expect (maxDiff < 0.0001f, "stems sum to the mix, maxDiff = " + juce::String (maxDiff));
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

        beginTest ("the exported stem WAVs sum to the exported mix WAV, choke and all");
        {
            // The in-memory nulls prove the renderer. This proves the thing a producer
            // actually does: drop the pad_NN.wav files into a DAW and expect them to add
            // back up to the mix WAV. It goes through the real 24-bit writer and reader.
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);

            // Keep the sum under 0 dBFS. A 24-bit WAV clips, and these stems are rendered
            // PRE-master, so there is no limiter to catch a hot sum -- at unity this very
            // pattern peaks at 1.04 and the mix file alone clamps to 1.0, which would make
            // the stems "fail" to sum for a reason that has nothing to do with stems.
            kit.pad (StarterKit::Kick).volume      = 0.4f;
            kit.pad (StarterKit::OpenHat).volume   = 0.4f;
            kit.pad (StarterKit::ClosedHat).volume = 0.4f;
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 3; p.bpm = 120.0;
            p.lane (0).targetPad = StarterKit::Kick;
            p.lane (0).length = 16; p.lane (0).step (0).on = true; p.lane (0).step (8).on = true;
            p.lane (1).targetPad = StarterKit::OpenHat;
            p.lane (1).length = 16; p.lane (1).step (0).on = true; p.lane (1).step (8).on = true;
            p.lane (2).targetPad = StarterKit::ClosedHat;
            p.lane (2).length = 16; p.lane (2).step (2).on = true; p.lane (2).step (10).on = true;

            // Pre-master, exactly as MainComponent exports stems: the master strip ends in
            // an always-on limiter, so a post-master stem could never sum.
            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 0.5;

            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("rollforge_stem_null_wav");
            dir.deleteRecursively();
            dir.createDirectory();
            const juce::File mixFile = dir.getChildFile ("mix.wav");

            expect (WavExporter::exportMix (engine, p, mixFile, opts));
            expect (WavExporter::exportStems (engine, p, dir, opts) == 3);

            juce::WavAudioFormat fmt;
            auto readAll = [&fmt] (const juce::File& f, juce::AudioBuffer<float>& into) -> bool
            {
                std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (f.createInputStream().release(), true));
                if (r == nullptr)
                    return false;
                into.setSize ((int) r->numChannels, (int) r->lengthInSamples);
                r->read (&into, 0, (int) r->lengthInSamples, 0, true, true);
                return true;
            };

            juce::AudioBuffer<float> mix;
            expect (readAll (mixFile, mix));
            const int n = mix.getNumSamples();
            expect (n > 0);
            expect (mix.getMagnitude (0, 0, n) > 0.0f);

            juce::AudioBuffer<float> sum (2, n);
            sum.clear();
            for (int pad = 0; pad < 16; ++pad)
            {
                const juce::File f = dir.getChildFile ("pad_" + juce::String (pad).paddedLeft ('0', 2) + ".wav");
                if (! f.existsAsFile())
                    continue;

                juce::AudioBuffer<float> stem;
                expect (readAll (f, stem));
                const int m = juce::jmin (n, stem.getNumSamples());
                for (int ch = 0; ch < 2; ++ch)
                    sum.addFrom (ch, 0, stem, ch, 0, m);
            }

            // 24-bit quantisation across the mix and three stems, so this cannot be as
            // tight as the in-memory nulls -- but it is still ~100x below audibility.
            // Guard the guard: if a future kit change pushes this back over 0 dBFS, the WAV
            // clips and the null below would fail for the wrong reason. Fail loudly here.
            expect (mix.getMagnitude (0, n) < 0.999f,
                    "the mix WAV does not clip, peak = " + juce::String (mix.getMagnitude (0, n)));

            float maxDiff = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (sum.getSample (ch, i) - mix.getSample (ch, i)));
            expect (maxDiff < 0.0001f, "stem WAVs sum to the mix WAV, maxDiff = " + juce::String (maxDiff));

            dir.deleteRecursively();
        }
    }
};

static StemNullTest stemNullTest;

} // namespace rollforge::tests
