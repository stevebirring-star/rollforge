// RollForge — Pattern model + TripleBuffer unit tests (Phase 2, commit 1).
//
// Headless: the sequencer data model (Step/Lane/Pattern defaults + accessors) and
// the lock-free triple buffer that hands a Pattern snapshot to the audio thread —
// it must deliver the LATEST published value and survive a full Pattern round-trip.

#include "engine/TripleBuffer.h"
#include "model/Pattern.h"

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
    }
};

static PatternModelTest patternModelTest;

} // namespace rollforge::tests
