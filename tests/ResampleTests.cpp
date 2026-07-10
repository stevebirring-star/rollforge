// RollForge — Resample tests (P2: bounce the loop onto a pad).
//
// Headless + pure. The point of folding is that a bounced bar keeps its own decay AND stays
// exactly one bar long. Both halves of that have to be true, and the second is the one a
// truncating implementation gets right by accident.

#include "engine/Resample.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    /** A buffer whose sample at index i is i + 1, so every sample is identifiable. */
    juce::AudioBuffer<float> ramp (int channels, int samples)
    {
        juce::AudioBuffer<float> b (channels, samples);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < samples; ++i)
                b.setSample (ch, i, (float) (i + 1));
        return b;
    }
}

class ResampleTest final : public juce::UnitTest
{
public:
    ResampleTest() : juce::UnitTest ("RollForge Resample", testCategory) {}

    void runTest() override
    {
        beginTest ("a bar of 4/4 is four beats, whatever the tempo");
        {
            expectEquals (Resample::loopSamples (44100.0, 120.0, 1), 88200);   // 2 s
            expectEquals (Resample::loopSamples (44100.0, 120.0, 4), 352800);  // 8 s
            expectEquals (Resample::loopSamples (44100.0, 60.0,  1), 176400);  // 4 s
            expectEquals (Resample::loopSamples (48000.0, 96.0,  2), 240000);  // 5 s

            // Nonsense in, something playable out — never a zero-length sample.
            expectEquals (Resample::loopSamples (0.0, 0.0, 0), 88200);
            expect (Resample::loopSamples (44100.0, 100000.0, 1) >= 1, "never zero samples");
        }

        beginTest ("the tail lands where the loop's next pass would have put it");
        {
            // 4-sample loop, 3 samples of tail. Sample 4 belongs at 0, 5 at 1, 6 at 2.
            auto b = ramp (1, 7);   // 1 2 3 4 | 5 6 7
            Resample::foldTailIntoLoop (b, 4);

            expectEquals (b.getNumSamples(), 4, "the result must be exactly one loop long");
            expectEquals (b.getSample (0, 0), 1.0f + 5.0f);
            expectEquals (b.getSample (0, 1), 2.0f + 6.0f);
            expectEquals (b.getSample (0, 2), 3.0f + 7.0f);
            expectEquals (b.getSample (0, 3), 4.0f, "nothing wrapped onto the last sample");
        }

        beginTest ("a tail longer than the loop wraps as many times as it needs to");
        {
            // A reverb can outlast the bar it decays from. Each pass of the loop would have
            // layered it again, so folding must too — `i % loop`, not `i - loop`.
            auto b = ramp (1, 9);   // loop 1 2 | tail 3 4 5 6 7 8 9
            Resample::foldTailIntoLoop (b, 2);

            expectEquals (b.getNumSamples(), 2);
            expectEquals (b.getSample (0, 0), 1.0f + 3.0f + 5.0f + 7.0f + 9.0f);
            expectEquals (b.getSample (0, 1), 2.0f + 4.0f + 6.0f + 8.0f);
        }

        beginTest ("every channel is folded, not just the first");
        {
            juce::AudioBuffer<float> b (2, 4);
            b.clear();
            b.setSample (0, 0, 1.0f);  b.setSample (0, 2, 0.25f);   // left:  head + tail
            b.setSample (1, 1, 1.0f);  b.setSample (1, 3, 0.50f);   // right: head + tail

            Resample::foldTailIntoLoop (b, 2);
            expectEquals (b.getNumSamples(), 2);
            expectEquals (b.getSample (0, 0), 1.25f);
            expectEquals (b.getSample (1, 1), 1.50f);
        }

        beginTest ("a buffer no longer than its loop is left exactly as it was");
        {
            auto b = ramp (1, 4);
            const float peak = Resample::foldTailIntoLoop (b, 4);
            expectEquals (b.getNumSamples(), 4);
            expectEquals (b.getSample (0, 0), 1.0f);
            expectEquals (b.getSample (0, 3), 4.0f);
            expectEquals (peak, 4.0f, "the returned peak is the peak of the result");

            auto shorter = ramp (1, 3);
            Resample::foldTailIntoLoop (shorter, 4);
            expectEquals (shorter.getNumSamples(), 3, "a short render is not stretched");
        }

        beginTest ("folding can push a loop past what the limiter allowed, and says so");
        {
            // The whole hazard: summing a tail onto a downbeat adds energy the render's
            // limiter never saw. The peak comes back so the caller can decide.
            juce::AudioBuffer<float> b (1, 4);
            b.clear();
            b.setSample (0, 0, 0.9f);   // a downbeat the limiter let through
            b.setSample (0, 2, 0.4f);   // a tail that lands on top of it

            const float peak = Resample::foldTailIntoLoop (b, 2);
            expectWithinAbsoluteError (peak, 1.3f, 1.0e-6f, "the fold clipped without telling us");
        }

        beginTest ("limitPeak scales a hot loop down, and leaves a quiet one alone");
        {
            juce::AudioBuffer<float> hot (1, 2);
            hot.setSample (0, 0, 1.3f);
            hot.setSample (0, 1, -0.65f);

            const float gain = Resample::limitPeak (hot, 0.99f);
            expectWithinAbsoluteError (gain, 0.99f / 1.3f, 1.0e-6f);
            expectWithinAbsoluteError (hot.getSample (0, 0), 0.99f, 1.0e-6f);
            expectWithinAbsoluteError (hot.getSample (0, 1), -0.495f, 1.0e-5f,
                                       "the whole loop scales, so nothing is squashed");

            juce::AudioBuffer<float> quiet (1, 2);
            quiet.setSample (0, 0, 0.5f);
            quiet.setSample (0, 1, 0.25f);
            expectEquals (Resample::limitPeak (quiet, 0.99f), 1.0f, "a quiet loop is untouched");
            expectEquals (quiet.getSample (0, 0), 0.5f);

            juce::AudioBuffer<float> silent (1, 4);
            silent.clear();
            expectEquals (Resample::limitPeak (silent, 0.99f), 1.0f, "silence must not divide by zero");
        }
    }
};

static ResampleTest resampleTest;

} // namespace rollforge::tests
