// RollForge — Capture tests (tap-to-pattern and beatbox share this quantiser).
//
// Headless + pure. The cases that matter are the ones a metronome-perfect test would never
// produce: a downbeat tapped early, a hit on a triplet lane, two taps rounding onto one step.

#include "model/Capture.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    /** 16 lanes, lane i plays pad i, one bar of straight 1/16ths, nothing lit. */
    Pattern grid()
    {
        return blankPattern();
    }

    constexpr double bpm120Step = 0.125;   // one 1/16 at 120 BPM = 2.0 s / 16
}

class CaptureTest final : public juce::UnitTest
{
public:
    CaptureTest() : juce::UnitTest ("RollForge Capture", testCategory) {}

    void runTest() override
    {
        beginTest ("a pad with no lane cannot be captured onto");
        {
            Pattern p = grid();
            int lane = -99, step = -99;
            expect (Capture::quantise (p, { 0.0, 0, 1.0f }, 120.0, lane, step));
            expectEquals (lane, 0);

            p.numLanes = 2;   // only pads 0 and 1 have lanes now
            expectEquals (Capture::laneForPad (p, 5), -1);
            expect (! Capture::quantise (p, { 0.0, 5, 1.0f }, 120.0, lane, step));

            const auto r = Capture::apply (p, { { 0.0, 5, 1.0f } }, {});
            expectEquals (r.dropped, 1);
            expectEquals (r.placed, 0);
        }

        beginTest ("hits round to the nearest step, not down to it");
        {
            Pattern p = grid();
            int lane = 0, step = 0;

            // 120 BPM: a 1/16 is 0.125 s. 0.07 s is past halfway to step 1.
            expect (Capture::quantise (p, { 0.06, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 0, "just early of step 0");
            expect (Capture::quantise (p, { 0.07, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 1, "past halfway belongs to step 1");
            expect (Capture::quantise (p, { 4 * bpm120Step + 0.01, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 4);
        }

        beginTest ("a downbeat tapped EARLY lands on the downbeat, not on the last step");
        {
            // Nobody taps a downbeat late. A quantiser that clamps instead of wrapping puts
            // their kick at the end of the bar, one loop out of place, and the beat limps.
            Pattern p = grid();
            int lane = 0, step = 0;

            const double barSeconds = 2.0;   // 120 BPM
            expect (Capture::quantise (p, { barSeconds - 0.004, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 0, "4 ms early is a downbeat");

            expect (Capture::quantise (p, { barSeconds - 0.07, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 15, "half a step early is still the last step");

            // ...and the mirror: a hit fractionally before zero.
            expect (Capture::quantise (p, { -0.004, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 0);
            expect (Capture::quantise (p, { -0.07, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 15, "just before the loop start is the end of the previous bar");
        }

        beginTest ("a hit past the end of the loop wraps, however many loops late");
        {
            Pattern p = grid();
            int lane = 0, step = 0;

            expect (Capture::quantise (p, { 2.0 + 3 * bpm120Step, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 3, "one bar and three steps in");
            expect (Capture::quantise (p, { 20.0 + 5 * bpm120Step, 0, 1.0f }, 120.0, lane, step));
            expectEquals (step, 5, "ten bars later, same step");
        }

        beginTest ("a triplet lane is quantised on twelve steps, not sixteen");
        {
            Pattern p = grid();
            p.lane (3).triplet = true;
            p.lane (3).length  = 12;

            int lane = 0, step = 0;
            const double tripletStep = 2.0 / 12.0;   // one bar / 12

            expect (Capture::quantise (p, { tripletStep * 5.0, 3, 1.0f }, 120.0, lane, step));
            expectEquals (lane, 3);
            expectEquals (step, 5, "the fifth triplet, not the nearest 1/16");

            // The same instant on a straight lane rounds somewhere else entirely. That is the
            // whole reason quantise() looks at the lane rather than at a global grid.
            expect (Capture::quantise (p, { tripletStep * 5.0, 2, 1.0f }, 120.0, lane, step));
            expectEquals (lane, 2);
            expectEquals (step, 7);
        }

        beginTest ("the tempo decides the grid");
        {
            Pattern p = grid();
            int lane = 0, step = 0;

            // At 60 BPM a bar is 4 s and a 1/16 is 0.25 s.
            expect (Capture::quantise (p, { 1.0, 0, 1.0f }, 60.0, lane, step));
            expectEquals (step, 4);

            // Nonsense tempo falls back to 120 rather than dividing by zero.
            expect (Capture::quantise (p, { 0.5, 0, 1.0f }, 0.0, lane, step));
            expectEquals (step, 4);
        }

        beginTest ("two taps on one step are one note, and the louder wins");
        {
            Pattern p = grid();
            const auto r = Capture::apply (p, {
                { 0.251, 0, 0.4f },    // both round to step 2
                { 0.249, 0, 0.9f },
            }, {});

            expectEquals (r.placed, 1);
            expectEquals (r.merged, 1);
            expect (p.lane (0).step (2).on);
            expectWithinAbsoluteError (p.lane (0).step (2).velocity, 0.9f, 1.0e-6f,
                                       "the flam overwrote the note");
        }

        beginTest ("a hit quieter than the floor is noise, not a note");
        {
            Pattern p = grid();
            Capture::Options opts;
            opts.minVelocity = 0.05f;

            const auto r = Capture::apply (p, { { 0.0, 0, 0.02f }, { 0.5, 1, 0.06f } }, opts);
            expectEquals (r.dropped, 1);
            expectEquals (r.placed, 1);
            expect (! p.lane (0).step (0).on);
            expect (p.lane (1).step (4).on);
        }

        beginTest ("a captured hit is a clean single tap, whatever was programmed there");
        {
            // You played a note. You did not play the four-ratchet 25%-probability roll that
            // Make a Beat happened to leave on that step.
            Pattern p = grid();
            Step& existing = p.lane (0).step (2);
            existing.on          = false;   // off, so the hit places rather than merges
            existing.ratchets    = 4;
            existing.ratchetRamp = 0.8f;
            existing.probability = 25;
            existing.microShift  = 0.3f;
            existing.sampleLock  = 2;

            Capture::apply (p, { { 0.25, 0, 0.7f } }, {});

            const Step& s = p.lane (0).step (2);
            expect (s.on);
            expectWithinAbsoluteError (s.velocity, 0.7f, 1.0e-6f);
            expectEquals (s.ratchets, 1, "an inherited ratchet turned one tap into a roll");
            expectEquals (s.probability, 100);
            expectWithinAbsoluteError (s.microShift, 0.0f, 1.0e-6f);
            expectEquals (s.sampleLock, -1);
            expectWithinAbsoluteError (s.ratchetRamp, 0.0f, 1.0e-6f);
        }

        beginTest ("overdub keeps what is there; replace clears the whole pattern first");
        {
            Pattern p = grid();
            p.lane (1).step (4).on = true;
            p.lane (1).step (4).velocity = 0.3f;

            Pattern over = p;
            Capture::apply (over, { { 0.0, 0, 1.0f } }, {});   // replace defaults to false
            expect (over.lane (0).step (0).on, "the new hit");
            expect (over.lane (1).step (4).on, "overdub erased what was already there");

            Pattern fresh = p;
            Capture::Options opts;
            opts.replace = true;
            Capture::apply (fresh, { { 0.0, 0, 1.0f } }, opts);
            expect (fresh.lane (0).step (0).on);
            expect (! fresh.lane (1).step (4).on, "replace kept the old pattern");
        }

        beginTest ("an empty hit list leaves the pattern exactly as it was");
        {
            Pattern p = grid();
            p.lane (2).step (6).on = true;

            const auto r = Capture::apply (p, {}, {});
            expectEquals (r.placed, 0);
            expectEquals (r.dropped, 0);
            expect (p.lane (2).step (6).on);
        }

        beginTest ("hits are clamped into a playable velocity, never past full");
        {
            Pattern p = grid();
            Capture::apply (p, { { 0.0, 0, 4.0f } }, {});
            expectWithinAbsoluteError (p.lane (0).step (0).velocity, 1.0f, 1.0e-6f);
        }
    }
};

static CaptureTest captureTest;

} // namespace rollforge::tests
