// RollForge — Song tests (P2: the arrangement chain).
//
// Headless + pure. Song mode reduces to one question asked once per bar — "which slot at
// bar B?" — so these tests are that question, asked at every boundary that could be off by
// one: the first bar of a step, the last, the wrap, and the bar past the end.

#include "model/Song.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class SongTest final : public juce::UnitTest
{
public:
    SongTest() : juce::UnitTest ("RollForge Song", testCategory) {}

    void runTest() override
    {
        beginTest ("an empty song has nothing to play at any bar");
        {
            Song song;
            expect (song.isEmpty());
            expectEquals (song.totalBars(), 0);
            expectEquals (song.slotAtBar (0), -1);
            expectEquals (song.stepAtBar (0), -1);
            expectEquals (song.slotAtBar (99), -1);
        }

        beginTest ("a negative bar is never a valid question");
        {
            Song song { { { 0, 4 } }, true };
            expectEquals (song.slotAtBar (-1), -1);
            expectEquals (song.stepAtBar (-1), -1);
        }

        beginTest ("each step covers exactly its own bars, first to last");
        {
            // A A B B B C  -> 6 bars.
            Song song { { { 0, 2 }, { 1, 3 }, { 2, 1 } }, true };
            expectEquals (song.totalBars(), 6);

            const int expected[6] = { 0, 0, 1, 1, 1, 2 };
            for (int bar = 0; bar < 6; ++bar)
                expectEquals (song.slotAtBar (bar), expected[bar],
                              "bar " + juce::String (bar) + " played the wrong slot");

            expectEquals (song.stepAtBar (1), 0, "the last bar of a step is still that step");
            expectEquals (song.stepAtBar (2), 1, "the first bar of a step must switch on time");
            expectEquals (song.stepAtBar (4), 1);
            expectEquals (song.stepAtBar (5), 2);
        }

        beginTest ("a looping song wraps, and keeps wrapping");
        {
            Song song { { { 0, 2 }, { 3, 1 } }, true };   // A A D
            expectEquals (song.totalBars(), 3);

            expectEquals (song.slotAtBar (3), 0, "the bar after the end is the first bar again");
            expectEquals (song.slotAtBar (4), 0);
            expectEquals (song.slotAtBar (5), 3);
            expectEquals (song.slotAtBar (3000), 0, "a long session must not drift");
            expectEquals (song.slotAtBar (3001), 0);
            expectEquals (song.slotAtBar (3002), 3);
        }

        beginTest ("a one-shot song ends, and stays ended");
        {
            Song song { { { 0, 2 }, { 1, 1 } }, false };
            expectEquals (song.slotAtBar (2), 1, "the last bar still plays");
            expectEquals (song.slotAtBar (3), -1, "the bar after the last must end the song");
            expectEquals (song.stepAtBar (3), -1);
            expectEquals (song.slotAtBar (10000), -1, "and it does not come back");
        }

        beginTest ("a step of zero bars is played for one, not silently swallowed");
        {
            // Nothing in the UI can make one, but a hand-edited file could, and a chain that
            // ate a step would be far more baffling than one that plays it briefly. Crucially
            // it also keeps totalBars() > 0, so the wrap below cannot divide by zero.
            Song song { { { 0, 0 }, { 1, 0 } }, true };
            expectEquals (song.totalBars(), 2);
            expectEquals (song.slotAtBar (0), 0);
            expectEquals (song.slotAtBar (1), 1);
            expectEquals (song.slotAtBar (2), 0, "the wrap must still work");

            Song negative { { { 2, -5 } }, true };
            expectEquals (negative.totalBars(), 1);
            expectEquals (negative.slotAtBar (0), 2);
            expectEquals (negative.slotAtBar (7), 2);
        }

        beginTest ("a step naming a slot outside A..H plays nothing rather than reading past the bank");
        {
            Song song { { { 99, 1 }, { -1, 1 } }, true };
            expectEquals (song.stepAtBar (0), 0, "the step still occupies its bar");
            expectEquals (song.slotAtBar (0), -1, "but it names no slot");
            expectEquals (song.slotAtBar (1), -1);
        }

        beginTest ("the same slot twice in a row is two steps, not one");
        {
            // The app asks for slotAtBar(bar + 1) and only queues a switch when it differs
            // from the slot playing now. A A must therefore not look like a switch at bar 1.
            Song song { { { 4, 1 }, { 4, 1 }, { 5, 1 } }, true };
            expectEquals (song.slotAtBar (0), 4);
            expectEquals (song.slotAtBar (1), 4);
            expectEquals (song.slotAtBar (2), 5);
            expectEquals (song.stepAtBar (0), 0);
            expectEquals (song.stepAtBar (1), 1, "the chip highlight must still advance");
        }
    }
};

static SongTest songTest;

} // namespace rollforge::tests
