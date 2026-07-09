// RollForge — WYSIWYG export guarantee (moat: "what you hear is what you export").
//
// A regression-gated promise: exports are a faithful image of the pattern — MIDI
// notes land on the exact step ticks with the step's velocity and NO doubling,
// the tempo is written exactly, and macro (MasterBus) edits are baked into the
// rendered audio. Rivals leak here (Atlas drops per-pad edits on drag-out,
// Playbeat's MIDI drifts); this locks it so RollForge never regresses into it.

#include "model/MidiExporter.h"
#include "engine/DrumEngine.h"
#include "engine/OfflineRenderer.h"
#include "library/KitInstaller.h"
#include "library/StarterKit.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <memory>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    int countNoteOns (const juce::MidiMessageSequence& seq)
    {
        int n = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
            if (seq.getEventPointer (i)->message.isNoteOn())
                ++n;
        return n;
    }

    // Number of note-ons at exactly (tick, note, velocity) — must be 1 for a match.
    int matches (const juce::MidiMessageSequence& seq, int tick, int note, int vel)
    {
        int n = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* e = seq.getEventPointer (i);
            if (e->message.isNoteOn()
                && (int) std::llround (e->message.getTimeStamp()) == tick
                && e->message.getNoteNumber() == note
                && e->message.getVelocity() == vel)
                ++n;
        }
        return n;
    }

    float maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int n  = juce::jmin (a.getNumSamples(), b.getNumSamples());
        const int ch = juce::jmin (a.getNumChannels(), b.getNumChannels());
        float d = 0.0f;
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < n; ++i)
                d = juce::jmax (d, std::abs (a.getSample (c, i) - b.getSample (c, i)));
        return d;
    }
}

class WysiwygExportTest final : public juce::UnitTest
{
public:
    WysiwygExportTest() : juce::UnitTest ("RollForge WysiwygExport", testCategory) {}

    void runTest() override
    {
        beginTest ("MIDI is a faithful, drift-free image of the pattern (no doubling)");
        {
            constexpr int tpq = 960, stepTicks = tpq / 4;   // 240 ticks per 1/16 step

            Pattern p;
            p.numLanes = 3; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;  p.lane (0).step (0).velocity  = 0.9f;
            p.lane (0).step (8).on = true;  p.lane (0).step (8).velocity  = 0.9f;
            p.lane (1).targetPad = 1; p.lane (1).length = 16;
            p.lane (1).step (4).on = true;  p.lane (1).step (4).velocity  = 0.7f;
            p.lane (1).step (12).on = true; p.lane (1).step (12).velocity = 0.7f;
            p.lane (2).targetPad = 2; p.lane (2).length = 16;
            p.lane (2).step (2).on = true;  p.lane (2).step (2).velocity  = 0.5f;

            const auto seq = MidiExporter::toSequence (p, 1, tpq);

            // Exactly one note per on-step — nothing dropped, nothing doubled.
            expectEquals (countNoteOns (seq), 5);

            // Each on-step lands on its exact tick, GM note and mapped velocity.
            struct Exp { int step, pad, vel; };
            const Exp expected[] = {
                { 0, 0, 114 }, { 8, 0, 114 },   // kick   (0.9 -> 114)
                { 4, 1, 89  }, { 12, 1, 89 },   // snare  (0.7 -> 89)
                { 2, 2, 64  },                  // c-hat  (0.5 -> 64)
            };
            for (const auto& e : expected)
                expectEquals (matches (seq, e.step * stepTicks,
                                       MidiExporter::gmNoteForPad (e.pad), e.vel), 1);
        }

        beginTest ("tempo is written exactly (no drift)");
        {
            Pattern p;
            p.numLanes = 1; p.bpm = 140.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16; p.lane (0).step (0).on = true;

            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_wysiwyg.mid");
            f.deleteFile();
            expect (MidiExporter::save (p, f, 1));

            juce::MidiFile mf;
            std::unique_ptr<juce::FileInputStream> is (f.createInputStream());
            expect (is != nullptr && mf.readFrom (*is));

            double spq = -1.0;
            for (int t = 0; t < mf.getNumTracks(); ++t)
            {
                const auto* trk = mf.getTrack (t);
                for (int e = 0; e < trk->getNumEvents(); ++e)
                    if (trk->getEventPointer (e)->message.isTempoMetaEvent())
                        spq = trk->getEventPointer (e)->message.getTempoSecondsPerQuarterNote();
            }
            expect (spq > 0.0);
            expect (std::abs (spq - 60.0 / 140.0) < 1.0e-4);   // 140 BPM, exact
            f.deleteFile();
        }

        beginTest ("macro (MasterBus) edits are baked into the rendered audio");
        {
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            for (int s = 0; s < 16; s += 4)
                p.lane (0).step (s).on = true;

            OfflineRenderer::Options clean;
            clean.applyMasterFx = true; clean.bars = 1; clean.tailSeconds = 0.3;

            OfflineRenderer::Options driven = clean;
            driven.drive = 1.0f;   // a real macro edit

            juce::AudioBuffer<float> a, b;
            OfflineRenderer::render (engine, p, a, clean);
            DrumEngine engine2; installKitIntoEngine (kit, engine2);
            OfflineRenderer::render (engine2, p, b, driven);

            expect (a.getMagnitude (0, 0, a.getNumSamples()) > 0.0f);
            // The Drive macro must actually change the export, not vanish on the way out.
            expect (maxAbsDiff (a, b) > 0.001f);
        }

        beginTest ("EVERY master-strip control is baked into the rendered audio");
        {
            // Testing one macro was not enough. Drive was baked and passed this file's
            // older test, while the EQ and the glue compressor — added later — were never
            // copied into OfflineRenderer::Options and so vanished from every export.
            // Exercise each control on its own, so adding a control without wiring it
            // through fails here rather than in someone's DAW.
            DrumEngine engine;
            Kit kit = StarterKit::build (44100.0);
            installKitIntoEngine (kit, engine);

            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            for (int s = 0; s < 16; s += 4)
                p.lane (0).step (s).on = true;

            OfflineRenderer::Options clean;
            clean.applyMasterFx = true; clean.bars = 1; clean.tailSeconds = 0.3;

            juce::AudioBuffer<float> reference;
            OfflineRenderer::render (engine, p, reference, clean);
            expect (reference.getMagnitude (0, 0, reference.getNumSamples()) > 0.0f);

            struct Control { const char* name; void (*apply) (OfflineRenderer::Options&); };
            const Control controls[] = {
                { "punch",  [] (OfflineRenderer::Options& o) { o.punch  =  1.0f; } },
                { "drive",  [] (OfflineRenderer::Options& o) { o.drive  =  1.0f; } },
                { "crush",  [] (OfflineRenderer::Options& o) { o.crush  =  1.0f; } },
                { "space",  [] (OfflineRenderer::Options& o) { o.space  =  1.0f; } },
                { "lowEq",  [] (OfflineRenderer::Options& o) { o.lowEq  = 12.0f; } },
                { "midEq",  [] (OfflineRenderer::Options& o) { o.midEq  = 12.0f; } },
                { "highEq", [] (OfflineRenderer::Options& o) { o.highEq = 12.0f; } },
                { "comp",   [] (OfflineRenderer::Options& o) { o.comp   =  1.0f; } },
            };

            for (const auto& c : controls)
            {
                OfflineRenderer::Options driven = clean;
                c.apply (driven);

                DrumEngine fresh; installKitIntoEngine (kit, fresh);
                juce::AudioBuffer<float> rendered;
                OfflineRenderer::render (fresh, p, rendered, driven);

                expect (maxAbsDiff (reference, rendered) > 0.001f,
                        juce::String (c.name) + " is not baked into the export");
            }
        }
    }
};

static WysiwygExportTest wysiwygExportTest;

} // namespace rollforge::tests
