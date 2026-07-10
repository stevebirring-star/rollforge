// RollForge — Pattern model + TripleBuffer unit tests (Phase 2, commit 1).
//
// Headless: the sequencer data model (Step/Lane/Pattern defaults + accessors) and
// the lock-free triple buffer that hands a Pattern snapshot to the audio thread —
// it must deliver the LATEST published value and survive a full Pattern round-trip.

#include "engine/TripleBuffer.h"
#include "model/Pattern.h"
#include "model/RollCompiler.h"
#include "model/RollPresets.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class PatternModelTest final : public juce::UnitTest
{
public:
    PatternModelTest() : juce::UnitTest ("RollForge PatternModel", testCategory) {}

    void runTest() override
    {
        beginTest ("Step / Lane / Pattern defaults");
        {
            Step s;
            expect (! s.on);
            expectWithinAbsoluteError (s.velocity, 0.8f, 1.0e-6f);
            expectWithinAbsoluteError (s.microShift, 0.0f, 1.0e-6f);
            expectEquals (s.ratchets, 1);
            expectEquals (s.probability, 100);
            expectEquals (s.sampleLock, -1);

            Lane l;
            expectEquals ((int) l.steps.size(), maxStepsPerLane);
            expectEquals (l.length, 16);
            expect (! l.triplet);
            expectEquals (l.targetPad, 0);

            Pattern p;
            expectEquals ((int) p.lanes.size(), maxLanes);
            expectEquals (p.numLanes, 0);
            expectWithinAbsoluteError (p.bpm, 120.0, 1.0e-9);
            expectWithinAbsoluteError (p.swing, 0.0f, 1.0e-6f);
        }

        beginTest ("Lane / Pattern accessors address the right elements");
        {
            Pattern p;
            p.numLanes = 2;
            p.lane (1).targetPad = 7;
            p.lane (1).step (3).on = true;
            p.lane (1).step (3).velocity = 0.55f;

            expectEquals (p.lane (1).targetPad, 7);
            expect (p.lanes[1].steps[3].on);
            expectWithinAbsoluteError (p.lanes[1].steps[3].velocity, 0.55f, 1.0e-6f);
        }

        beginTest ("patternBars() sizes exports to the longest lane (ceil to bars, >= 1)");
        {
            Pattern empty;
            expectEquals (patternBars (empty), 1);            // no lanes -> one bar

            Pattern oneBar;
            oneBar.numLanes = 2;
            oneBar.lane (0).length = 16;
            oneBar.lane (1).length = 8;
            expectEquals (patternBars (oneBar), 1);           // 16 steps == 1 bar

            Pattern twoBars = oneBar;
            twoBars.lane (1).length = 17;                     // just over a bar -> ceil to 2
            expectEquals (patternBars (twoBars), 2);

            Pattern fourBars = oneBar;
            fourBars.lane (0).length = 64;                    // max lane length -> 4 bars
            expectEquals (patternBars (fourBars), 4);

            // A triplet lane's length counts ITS steps, each 4/3 of a 1/16. Twelve of them
            // are a whole bar, not three quarters of one — measure the lane in GLOBAL steps
            // or a multi-bar triplet lane exports with its last bar silently truncated.
            Pattern triplet;
            triplet.numLanes = 1;
            triplet.lane (0).triplet = true;

            triplet.lane (0).length = 12;                     // 12 * 4/3 = 16 global -> 1 bar
            expectEquals (patternBars (triplet), 1);

            triplet.lane (0).length = 24;                     // -> 32 global -> 2 bars
            expectEquals (patternBars (triplet), 2);

            triplet.lane (0).length = 48;                     // -> 64 global -> 4 bars
            expectEquals (patternBars (triplet), 4);

            triplet.lane (0).length = 13;                     // -> ceil(17.33) = 18 -> 2 bars
            expectEquals (patternBars (triplet), 2);
        }

        beginTest ("TripleBuffer returns the default before any publish");
        {
            TripleBuffer<int> tb;
            expectEquals (tb.read(), 0);
        }

        beginTest ("TripleBuffer delivers the latest published value");
        {
            TripleBuffer<int> tb;

            tb.writeBuffer() = 10;
            tb.publish();
            expectEquals (tb.read(), 10);

            tb.writeBuffer() = 20;
            tb.publish();
            tb.writeBuffer() = 30;
            tb.publish();
            expectEquals (tb.read(), 30);   // latest wins (20 skipped)
            expectEquals (tb.read(), 30);   // nothing new -> unchanged
        }

        beginTest ("TripleBuffer round-trips a whole Pattern snapshot");
        {
            TripleBuffer<Pattern> tb;

            Pattern p;
            p.bpm = 128.0;
            p.swing = 0.3f;
            p.numLanes = 3;
            p.lane (0).targetPad = 5;
            p.lane (0).step (2).on = true;
            p.lane (0).step (2).velocity = 0.6f;
            p.lane (0).step (2).ratchets = 4;

            tb.writeBuffer() = p;
            tb.publish();

            const Pattern& got = tb.read();
            expectWithinAbsoluteError (got.bpm, 128.0, 1.0e-9);
            expectWithinAbsoluteError (got.swing, 0.3f, 1.0e-6f);
            expectEquals (got.numLanes, 3);
            expectEquals (got.lane (0).targetPad, 5);
            expect (got.lane (0).step (2).on);
            expectWithinAbsoluteError (got.lane (0).step (2).velocity, 0.6f, 1.0e-6f);
            expectEquals (got.lane (0).step (2).ratchets, 4);
        }

        // rollExtent is what draws a roll on the grid. It used to not exist, so a roll
        // that FillEngine generated played but was never drawn — and "Clear Rolls" then
        // looked like a dead button because it removed something invisible.
        beginTest ("rollExtent finds the lane firing the roll's pad");
        {
            Pattern p = blankPattern();
            p.lane (1).targetPad = 7;
            p.rolls[0] = RollCompiler::compile (RollPresets::make (RollPresets::MachineGun, 14.0, 2.0, 7));
            p.numRolls = 1;

            const RollExtent e = rollExtent (p, 0);
            expectEquals (e.lane, 1);
            expectEquals (e.startStep, 14);
            expectEquals (e.lengthSteps, 2);   // hits span columns 14 and 15
        }

        beginTest ("rollExtent draws nothing it cannot place");
        {
            Pattern p = blankPattern();
            p.rolls[0] = RollCompiler::compile (RollPresets::make (RollPresets::MachineGun, 0.0, 2.0, 3));
            p.numRolls = 1;

            expectEquals (rollExtent (p, -1).lane, -1);   // out of range
            expectEquals (rollExtent (p,  1).lane, -1);   // past numRolls
            expectEquals (rollExtent (p,  0).lane, 3);    // blankPattern maps lane i -> pad i

            Pattern noLane = p;
            noLane.numLanes = 2;                          // no active lane targets pad 3
            expectEquals (rollExtent (noLane, 0).lane, -1);

            Pattern silent = p;
            silent.rolls[0].count = 0;                    // a roll with no hits draws nothing
            expectEquals (rollExtent (silent, 0).lane, -1);
        }

        beginTest ("rollExtent clamps a roll to the lane it lives on");
        {
            Pattern p = blankPattern();
            p.rolls[0] = RollCompiler::compile (RollPresets::make (RollPresets::MachineGun, 15.0, 4.0, 0));
            p.numRolls = 1;

            const RollExtent e = rollExtent (p, 0);
            expectEquals (e.lane, 0);
            expectEquals (e.startStep, 15);
            expectEquals (e.lengthSteps, 1);   // a 16-step lane leaves room for one column

            Pattern past = p;
            past.rolls[0].startStep = 99.0f;   // a corrupt/out-of-range start still draws in-bounds
            const RollExtent c = rollExtent (past, 0);
            expect (c.startStep >= 0 && c.startStep < past.lane (0).length);
            expect (c.startStep + c.lengthSteps <= past.lane (0).length);
        }
    }
};

static PatternModelTest patternModelTest;

} // namespace rollforge::tests
