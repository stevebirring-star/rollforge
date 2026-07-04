// RollForge — MidiExporter tests (Phase 6, commit 2).
//
// Headless: steps become GM drum notes, ratchets flatten into multiple notes, and
// a saved MIDI file reads back with the right note count.

#include "model/MidiExporter.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

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
}

class MidiExportTest final : public juce::UnitTest
{
public:
    MidiExportTest() : juce::UnitTest ("RollForge MidiExport", testCategory) {}

    void runTest() override
    {
        beginTest ("GM map + basic step export");
        {
            expectEquals (MidiExporter::gmNoteForPad (0), 36);   // kick
            expectEquals (MidiExporter::gmNoteForPad (1), 38);   // snare

            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 0;
            p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (4).on = true;

            const auto seq = MidiExporter::toSequence (p, 1, 960);
            expectEquals (countNoteOns (seq), 2);

            bool sawKick = false;
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto& m = seq.getEventPointer (i)->message;
                if (m.isNoteOn() && m.getNoteNumber() == 36)
                    sawKick = true;
            }
            expect (sawKick);
        }

        beginTest ("ratchets flatten into multiple notes");
        {
            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 1;
            p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (0).ratchets = 4;

            expectEquals (countNoteOns (MidiExporter::toSequence (p, 1, 960)), 4);
        }

        beginTest ("save writes a readable MIDI file");
        {
            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (8).on = true;

            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_test.mid");
            f.deleteFile();
            expect (MidiExporter::save (p, f, 1));
            expect (f.existsAsFile());

            juce::MidiFile mf;
            std::unique_ptr<juce::FileInputStream> is (f.createInputStream());
            expect (is != nullptr);
            expect (mf.readFrom (*is));

            int total = 0;
            for (int t = 0; t < mf.getNumTracks(); ++t)
            {
                const auto* trk = mf.getTrack (t);
                for (int e = 0; e < trk->getNumEvents(); ++e)
                    if (trk->getEventPointer (e)->message.isNoteOn())
                        ++total;
            }
            expectEquals (total, 2);
            f.deleteFile();
        }
    }
};

static MidiExportTest midiExportTest;

} // namespace rollforge::tests
