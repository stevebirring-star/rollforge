// RollForge — Autosave tests (Phase 7, commit 1).
//
// Headless: a recovery file saves / is detected / loads / clears; a missing or
// empty file is not treated as a recovery.

#include "app/Autosave.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class AutosaveTest final : public juce::UnitTest
{
public:
    AutosaveTest() : juce::UnitTest ("RollForge Autosave", testCategory) {}

    void runTest() override
    {
        beginTest ("recovery save / detect / load / clear round-trip");
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_recovery_test.rollforge");
            f.deleteFile();
            expect (! Autosave::hasRecovery (f));

            Project p;
            p.bpm = 133.0;
            p.pads[0].samplePath = "/x/kick.wav";
            expect (Autosave::save (p, f));
            expect (Autosave::hasRecovery (f));

            Project q;
            expect (Autosave::load (f, q));
            expectWithinAbsoluteError ((float) q.bpm, 133.0f, 1.0e-3f);
            expect (q.pads[0].samplePath == juce::String ("/x/kick.wav"));

            Autosave::clear (f);
            expect (! Autosave::hasRecovery (f));
        }

        beginTest ("no recovery for a missing or empty file");
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_recovery_empty.rollforge");
            f.deleteFile();
            expect (! Autosave::hasRecovery (f));

            f.create();   // empty file
            expect (! Autosave::hasRecovery (f));
            f.deleteFile();
        }
    }
};

static AutosaveTest autosaveTest;

} // namespace rollforge::tests
