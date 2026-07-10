// RollForge — ProjectIO round-trip tests (Phase 6, commit 1).
//
// Headless: a Project survives JSON save -> load unchanged (pads, pattern steps,
// rolls, FX macros, transport); malformed input fails gracefully; file I/O works.

#include "model/ProjectIO.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

namespace
{
    /** The same JSON with `keys` removed — how a file written by an older build looks. */
    juce::var stripKeys (const juce::var& root, const juce::StringArray& keys)
    {
        auto* object = new juce::DynamicObject();
        if (auto* src = root.getDynamicObject())
            for (const auto& prop : src->getProperties())
                if (! keys.contains (prop.name.toString()))
                    object->setProperty (prop.name, prop.value);
        return juce::var (object);
    }
}

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

        beginTest ("the A..H bank round-trips, and the live pattern is slot 0's twin");
        {
            Project p;
            p.pattern.numLanes = 1;
            p.pattern.lane (0).step (0).on = true;

            for (int i = 0; i < numPatternSlots; ++i)
            {
                p.slots[(std::size_t) i].numLanes = 1;
                p.slots[(std::size_t) i].lane (0).length = 8 + i;      // each slot distinguishable
                p.slots[(std::size_t) i].lane (0).step (i).on = true;
            }
            p.currentSlot = 5;

            Project loaded;
            expect (ProjectIO::fromJson (ProjectIO::toJson (p), loaded));
            expectEquals (loaded.currentSlot, 5);
            for (int i = 0; i < numPatternSlots; ++i)
            {
                expectEquals (loaded.slots[(std::size_t) i].lane (0).length, 8 + i,
                              "slot " + juce::String (i) + " did not round-trip");
                expect (loaded.slots[(std::size_t) i].lane (0).step (i).on);
            }
        }

        beginTest ("a project saved before the bank existed still loads its groove into slot A");
        {
            // The exact shape of an old file: a "pattern", no "slots", no "currentSlot".
            Project old;
            old.pattern.numLanes = 1;
            old.pattern.lane (0).length = 12;
            old.pattern.lane (0).step (3).on = true;

            auto json = ProjectIO::toJson (old);
            json = juce::JSON::toString (stripKeys (juce::JSON::parse (json), { "slots", "currentSlot" }));

            Project loaded;
            expect (ProjectIO::fromJson (json, loaded));
            expectEquals (loaded.currentSlot, 0);
            expectEquals (loaded.slots[0].lane (0).length, 12, "the old groove was lost");
            expect (loaded.slots[0].lane (0).step (3).on);
            expect (patternIsEmpty (loaded.slots[1]), "the other seven slots must start empty");
        }

        beginTest ("the song chain round-trips, and a bad slot never reaches the app");
        {
            Project p;
            p.song.steps = { { 0, 2 }, { 3, 1 }, { 0, 4 } };
            p.song.loop  = false;
            p.songMode   = true;

            Project loaded;
            expect (ProjectIO::fromJson (ProjectIO::toJson (p), loaded));
            expect (loaded.songMode);
            expect (! loaded.song.loop);
            expectEquals ((int) loaded.song.steps.size(), 3);
            expectEquals (loaded.song.steps[1].slot, 3);
            expectEquals (loaded.song.steps[2].bars, 4);
            expectEquals (loaded.song.totalBars(), 7);

            // A hand-edited file naming slot 99: dropped on the way in, not once per bar.
            auto json = ProjectIO::toJson (p);
            json = json.replace ("\"slot\": 3", "\"slot\": 99");
            Project tampered;
            expect (ProjectIO::fromJson (json, tampered));
            expectEquals ((int) tampered.song.steps.size(), 2, "the impossible step survived");
            for (const auto& step : tampered.song.steps)
                expect (PatternBank::isValidSlot (step.slot));
        }

        beginTest ("song mode cannot come back on with an empty chain");
        {
            Project p;
            p.songMode = true;          // and no steps: there is nothing for it to drive

            Project loaded;
            expect (ProjectIO::fromJson (ProjectIO::toJson (p), loaded));
            expect (! loaded.songMode, "an empty chain must not arm song mode");
        }

        beginTest ("a project saved before arrangements existed loads with no chain");
        {
            Project old;
            auto json = ProjectIO::toJson (old);
            json = juce::JSON::toString (stripKeys (juce::JSON::parse (json), { "song" }));

            Project loaded;
            expect (ProjectIO::fromJson (json, loaded));
            expect (loaded.song.isEmpty());
            expect (! loaded.songMode);
            expect (loaded.song.loop, "loop defaults on");
        }

        beginTest ("blankPattern is empty but playable");
        {
            const auto blank = blankPattern();
            expect (patternIsEmpty (blank), "a blank slot must look empty");
            expectEquals (blank.numLanes, maxLanes,
                          "a default Pattern has no lanes: its grid would light silent steps");
            for (int i = 0; i < maxLanes; ++i)
            {
                expectEquals (blank.lane (i).targetPad, i, "lane i must fire pad i");
                expectEquals (blank.lane (i).length, straightStepsPerBar);
                expect (! blank.lane (i).triplet);
            }
            expectEquals (patternBars (blank), 1);
        }

        beginTest ("patternIsEmpty tells a written slot from a blank one");
        {
            Pattern blank;
            expect (patternIsEmpty (blank), "a default-constructed pattern has nothing in it");

            Pattern lanesButNoSteps;
            lanesButNoSteps.numLanes = 4;
            expect (patternIsEmpty (lanesButNoSteps), "lanes with no lit step are still silence");

            Pattern written = lanesButNoSteps;
            written.lane (2).step (7).on = true;
            expect (! patternIsEmpty (written));

            // A step past the lane's length never sounds, so it does not count.
            Pattern beyondEnd = lanesButNoSteps;
            beyondEnd.lane (0).length = 4;
            beyondEnd.lane (0).step (9).on = true;
            expect (patternIsEmpty (beyondEnd), "a step beyond the lane end is inaudible");

            // A roll on its own is enough to make a slot non-empty.
            Pattern rollOnly;
            rollOnly.numRolls = 1;
            expect (! patternIsEmpty (rollOnly));
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
