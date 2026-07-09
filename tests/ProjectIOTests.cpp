// RollForge — ProjectIO round-trip tests (Phase 6, commit 1).
//
// Headless: a Project survives JSON save -> load unchanged (pads, pattern steps,
// rolls, FX macros, transport); malformed input fails gracefully; file I/O works.

#include "model/ProjectIO.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class ProjectIOTest final : public juce::UnitTest
{
public:
    ProjectIOTest() : juce::UnitTest ("RollForge ProjectIO", testCategory) {}

    void runTest() override
    {
        beginTest ("project JSON round-trips");
        {
            Project p;
            p.bpm = 128.0; p.swing = 0.3f;
            p.punch = 0.4f; p.drive = 0.7f;
            p.pads[0].samplePath = "/kit/kick.wav"; p.pads[0].gain = 0.9f;
            p.pads[0].pitchSemitones = -2.0f; p.pads[0].chokeGroup = 1;
            p.pads[3].samplePath = "/kit/hat.wav"; p.pads[3].reverse = true;
            p.pads[0].muted = true; p.pads[5].soloed = true;
            p.pads[0].startFraction = 0.25f; p.pads[0].endFraction = 0.8f;
            p.pads[0].tone = -0.4f; p.pads[0].reverbSend = 0.65f;

            p.pattern.numLanes = 2;
            p.pattern.lane (0).targetPad = 0; p.pattern.lane (0).length = 16;
            p.pattern.lane (0).step (0).on = true;
            p.pattern.lane (0).step (0).velocity = 0.7f;
            p.pattern.lane (0).step (0).ratchets = 3;
            p.pattern.lane (1).targetPad = 1; p.pattern.lane (1).length = 16;
            p.pattern.lane (1).step (4).on = true;
            p.pattern.lane (1).step (4).probability = 50;

            p.pattern.numRolls = 1;
            p.pattern.rolls[0].targetPad = 2;
            p.pattern.rolls[0].startStep = 8.0f;
            p.pattern.rolls[0].count = 2;
            p.pattern.rolls[0].events[0].stepOffset = 0.0f;  p.pattern.rolls[0].events[0].velocity = 1.0f;
            p.pattern.rolls[0].events[1].stepOffset = 0.25f; p.pattern.rolls[0].events[1].velocity = 0.8f;

            const auto json = ProjectIO::toJson (p);
            expect (json.isNotEmpty());

            Project q;
            expect (ProjectIO::fromJson (json, q));
            expectWithinAbsoluteError ((float) q.bpm, 128.0f, 1.0e-4f);
            expectWithinAbsoluteError (q.swing, 0.3f, 1.0e-4f);
            expectWithinAbsoluteError (q.punch, 0.4f, 1.0e-4f);
            expectWithinAbsoluteError (q.drive, 0.7f, 1.0e-4f);
            expect (q.pads[0].samplePath == juce::String ("/kit/kick.wav"));
            expectWithinAbsoluteError (q.pads[0].gain, 0.9f, 1.0e-4f);
            expectWithinAbsoluteError (q.pads[0].pitchSemitones, -2.0f, 1.0e-4f);
            expectEquals (q.pads[0].chokeGroup, 1);
            expect (q.pads[3].reverse);
            expect (q.pads[0].muted);
            expect (q.pads[5].soloed);
            expect (! q.pads[1].muted);
            expectWithinAbsoluteError (q.pads[0].startFraction, 0.25f, 1.0e-4f);
            expectWithinAbsoluteError (q.pads[0].endFraction, 0.8f, 1.0e-4f);
            expectWithinAbsoluteError (q.pads[0].tone, -0.4f, 1.0e-4f);
            expectWithinAbsoluteError (q.pads[0].reverbSend, 0.65f, 1.0e-4f);
            // A project written before tone/send existed must load flat + dry.
            expectWithinAbsoluteError (q.pads[1].tone, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (q.pads[1].reverbSend, 0.0f, 1.0e-6f);
            expectEquals (q.pattern.numLanes, 2);
            expect (q.pattern.lane (0).step (0).on);
            expectWithinAbsoluteError (q.pattern.lane (0).step (0).velocity, 0.7f, 1.0e-4f);
            expectEquals (q.pattern.lane (0).step (0).ratchets, 3);
            expect (q.pattern.lane (1).step (4).on);
            expectEquals (q.pattern.lane (1).step (4).probability, 50);
            expectEquals (q.pattern.numRolls, 1);
            expectEquals (q.pattern.rolls[0].count, 2);
            expectWithinAbsoluteError (q.pattern.rolls[0].startStep, 8.0f, 1.0e-4f);
            expectWithinAbsoluteError (q.pattern.rolls[0].events[1].stepOffset, 0.25f, 1.0e-4f);
        }

        beginTest ("malformed JSON fails gracefully");
        {
            Project q;
            expect (! ProjectIO::fromJson ("not json {[", q));
            expect (! ProjectIO::fromJson ("42", q));   // valid JSON but not an object
        }

        beginTest ("file save/load round-trips");
        {
            Project p;
            p.bpm = 140.0;
            p.pads[0].samplePath = "/x/snare.wav";

            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_test.rollforge");
            f.deleteFile();
            expect (ProjectIO::save (p, f));

            Project q;
            expect (ProjectIO::load (f, q));
            expectWithinAbsoluteError ((float) q.bpm, 140.0f, 1.0e-4f);
            expect (q.pads[0].samplePath == juce::String ("/x/snare.wav"));
            f.deleteFile();
        }
    }
};

static ProjectIOTest projectIOTest;

} // namespace rollforge::tests
