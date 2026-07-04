// RollForge — Pad + Kit model unit tests.
//
// Pure, headless checks of the pad/kit data model: neutral defaults, sample
// slot queries, the 4x4 grid coordinate mapping, and deep-copy semantics (so a
// Kit can later serve as an undo snapshot). No audio device required.

#include "model/Kit.h"
#include "model/Pad.h"

namespace rollforge::tests
{

// Every RollForge test registers under this category (see TestMain.cpp).
static constexpr const char* testCategory = "rollforge";

namespace
{
    SampleBuffer::Ptr makeTestBuffer()
    {
        juce::AudioBuffer<float> audio (1, 4);
        audio.clear();
        return new SampleBuffer (std::move (audio), 44100.0, "test");
    }
}

class PadKitTest final : public juce::UnitTest
{
public:
    PadKitTest() : juce::UnitTest ("RollForge Pad/Kit", testCategory) {}

    void runTest() override
    {
        beginTest ("Pad has neutral defaults and no sample");
        {
            Pad pad;
            expectWithinAbsoluteError (pad.volume, 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (pad.pan, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (pad.pitchSemis, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (pad.attackMs, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (pad.releaseMs, 0.0f, 1.0e-6f);
            expectEquals (pad.chokeGroup, noChokeGroup);
            expect (! pad.reverse);
            expect (! pad.hasSample());
            expectEquals (pad.numAlternates(), 0);
            expect (pad.primarySample() == nullptr);
        }

        beginTest ("Pad sample-slot queries reflect assigned alternates");
        {
            Pad pad;
            auto primary = makeTestBuffer();
            pad.alternates[0] = primary;
            pad.alternates[2] = makeTestBuffer();   // a gap at [1] is allowed

            expect (pad.hasSample());
            expectEquals (pad.numAlternates(), 2);
            expect (pad.primarySample() == primary);
        }

        beginTest ("Kit is a 16-pad 4x4 grid with correct coordinate mapping");
        {
            Kit kit;
            expectEquals (kitNumPads, 16);
            expectEquals ((int) kit.pads.size(), 16);
            expect (kit.name == "Empty Kit");
            expect (! kit.hasAnySample());

            // row-major: index = row * 4 + column
            expectEquals (Kit::indexFor (0, 0), 0);
            expectEquals (Kit::indexFor (3, 0), 3);
            expectEquals (Kit::indexFor (0, 1), 4);
            expectEquals (Kit::indexFor (3, 3), 15);

            expect (Kit::isValidIndex (0));
            expect (Kit::isValidIndex (15));
            expect (! Kit::isValidIndex (-1));
            expect (! Kit::isValidIndex (16));

            // pad(column,row) and pad(index) address the same object.
            kit.pad (2, 1).volume = 0.25f;
            expectWithinAbsoluteError (kit.pad (Kit::indexFor (2, 1)).volume, 0.25f, 1.0e-6f);

            kit.pad (5).alternates[0] = makeTestBuffer();
            expect (kit.hasAnySample());
        }

        beginTest ("copying a Kit deep-copies pads and shares samples");
        {
            Kit original;
            auto sample = makeTestBuffer();          // refcount 1
            original.pad (0).alternates[0] = sample; // refcount 2 (local + pad)
            original.pad (0).volume = 0.5f;
            original.name = "Kit A";

            Kit copy = original;                     // deep-copies pads; increfs the sample

            // The sample is SHARED (reference counted), not duplicated.
            expect (copy.pad (0).primarySample() == sample);
            expectEquals (sample->getReferenceCount(), 3);   // local + original + copy

            // Editing the copy must not touch the original (independent pads).
            copy.pad (0).volume = 0.9f;
            copy.name = "Kit B";
            expectWithinAbsoluteError (original.pad (0).volume, 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (copy.pad (0).volume, 0.9f, 1.0e-6f);
            expect (original.name == "Kit A");
            expect (copy.name == "Kit B");
        }
    }
};

static PadKitTest padKitTest;

} // namespace rollforge::tests
