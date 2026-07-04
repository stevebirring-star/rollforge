// RollForge — PadMapping unit tests (pure input -> pad mapping).

#include "engine/PadMapping.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class PadMappingTest final : public juce::UnitTest
{
public:
    PadMappingTest() : juce::UnitTest ("RollForge PadMapping", testCategory) {}

    void runTest() override
    {
        beginTest ("MIDI note maps linearly to pad (36..51 -> 0..15)");
        {
            expectEquals (midiNoteToPad (36), 0);
            expectEquals (midiNoteToPad (43), 7);
            expectEquals (midiNoteToPad (51), 15);
            expectEquals (midiNoteToPad (35), -1);   // below range
            expectEquals (midiNoteToPad (52), -1);   // above range
            expectEquals (midiNoteToPad (0), -1);
        }

        beginTest ("keyboard char maps to the grid layout");
        {
            expectEquals (keyCharToPad ('1'), 0);
            expectEquals (keyCharToPad ('4'), 3);
            expectEquals (keyCharToPad ('q'), 4);
            expectEquals (keyCharToPad ('r'), 7);
            expectEquals (keyCharToPad ('a'), 8);
            expectEquals (keyCharToPad ('f'), 11);
            expectEquals (keyCharToPad ('z'), 12);
            expectEquals (keyCharToPad ('v'), 15);
            expectEquals (keyCharToPad ('k'), -1);   // unmapped
            expectEquals (keyCharToPad (' '), -1);   // space is reserved
            expectEquals (keyCharToPad ('Q'), -1);   // caller lowercases; uppercase unmapped
        }
    }
};

static PadMappingTest padMappingTest;

} // namespace rollforge::tests
