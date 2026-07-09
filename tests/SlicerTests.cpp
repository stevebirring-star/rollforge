// RollForge — Slicer tests (Credibility tier: slice-loop-to-pads).
//
// Headless + pure: onset detection on synthesised breakbeats, and the [start, end)
// tiling contract that lets each slice drop straight into a pad's trim region.

#include "library/Slicer.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double sr = 44100.0;

    /** An exponentially-decaying noise burst — a plausible drum hit. */
    void addHit (std::vector<float>& x, int atSample, float amplitude, double decaySeconds)
    {
        juce::Random rng ((juce::int64) atSample + 1);
        const int len = (int) (decaySeconds * sr);
        for (int i = 0; i < len; ++i)
        {
            const int j = atSample + i;
            if (j < 0 || j >= (int) x.size())
                break;
            const float envelope = std::exp (-(float) i / (float) (decaySeconds * sr * 0.25));
            x[(std::size_t) j] += amplitude * envelope * (rng.nextFloat() * 2.0f - 1.0f);
        }
    }

    /** A decaying low-frequency sine — a kick drum. Its period (18 ms at 55 Hz) is far
        longer than the envelope hop, so a naive detector sees the envelope ripple and
        splits one kick into many onsets. This is the case real drums hit and broadband
        noise bursts do not. */
    void addSineHit (std::vector<float>& x, int atSample, float amplitude,
                     double decaySeconds, double frequency)
    {
        const int len = (int) (decaySeconds * sr * 4);
        for (int i = 0; i < len; ++i)
        {
            const int j = atSample + i;
            if (j < 0 || j >= (int) x.size())
                break;
            const double t = (double) i / sr;
            x[(std::size_t) j] += amplitude * (float) (std::exp (-t / decaySeconds)
                                                       * std::sin (2.0 * juce::MathConstants<double>::pi * frequency * t));
        }
    }

    /** `numHits` equally-spaced hits across `seconds` of audio. */
    std::vector<float> makeLoop (int numHits, double seconds, float amplitude = 0.8f)
    {
        std::vector<float> x ((std::size_t) (seconds * sr), 0.0f);
        const int spacing = (int) x.size() / numHits;
        for (int h = 0; h < numHits; ++h)
            addHit (x, h * spacing, amplitude, 0.05);
        return x;
    }

    void checkTiling (juce::UnitTest& t, const std::vector<Slice>& slices)
    {
        t.expect (! slices.empty(), "at least one slice");
        t.expectWithinAbsoluteError (slices.front().startFraction, 0.0f, 1.0e-6f);
        t.expectWithinAbsoluteError (slices.back().endFraction, 1.0f, 1.0e-6f);

        for (std::size_t i = 0; i < slices.size(); ++i)
        {
            t.expect (slices[i].endFraction > slices[i].startFraction,
                      "slice " + juce::String ((int) i) + " is non-empty");
            if (i + 1 < slices.size())
                t.expectWithinAbsoluteError (slices[i].endFraction, slices[i + 1].startFraction, 1.0e-6f);
        }
    }
}

class SlicerTest final : public juce::UnitTest
{
public:
    SlicerTest() : juce::UnitTest ("RollForge Slicer", testCategory) {}

    void runTest() override
    {
        beginTest ("null / empty / silent input is safe and reports no onsets");
        {
            expect (Slicer::detectOnsets (nullptr, 100, sr).empty());

            std::vector<float> empty;
            expect (Slicer::detectOnsets (empty.data(), 0, sr).empty());

            std::vector<float> silence (4410, 0.0f);
            expect (Slicer::detectOnsets (silence.data(), (int) silence.size(), sr).empty());

            // A zero sample rate must not divide by zero.
            expect (Slicer::detectOnsets (silence.data(), (int) silence.size(), 0.0).empty());
        }

        beginTest ("onsets are found at each hit of an evenly-spaced loop");
        {
            const auto loop = makeLoop (8, 2.0);
            const auto onsets = Slicer::detectOnsets (loop.data(), (int) loop.size(), sr);

            expectEquals ((int) onsets.size(), 8);
            expectEquals (onsets.front(), 0);

            const int spacing = (int) loop.size() / 8;
            for (std::size_t i = 0; i < onsets.size(); ++i)
            {
                const int expected = (int) i * spacing;
                // Within one 10 ms window of the true hit (envelope hop + backtrack).
                expect (std::abs (onsets[i] - expected) < (int) (0.010 * sr),
                        "onset " + juce::String ((int) i) + " at " + juce::String (onsets[i])
                            + ", expected ~" + juce::String (expected));
            }

            for (std::size_t i = 1; i < onsets.size(); ++i)
                expect (onsets[i] > onsets[i - 1], "onsets strictly ascend");
        }

        beginTest ("a low-frequency kick is ONE onset, not a dozen (envelope ripple)");
        {
            // A 55 Hz kick has an 18 ms period vs a 2.9 ms hop, so its RMS envelope
            // oscillates all the way down the decay. Without the peak follower each
            // crest reads as a fresh onset. Regression: a real break sliced into 14.
            std::vector<float> x ((std::size_t) (1.0 * sr), 0.0f);
            addSineHit (x, 0, 0.95f, 0.12, 55.0);

            const auto onsets = Slicer::detectOnsets (x.data(), (int) x.size(), sr);
            expectEquals ((int) onsets.size(), 1);
            expectEquals (onsets.front(), 0);
        }

        beginTest ("a mixed kick/snare/hat break yields exactly one onset per hit");
        {
            // The shape of the loop that exposed the bug in the running app: sine kicks
            // and a tonal snare, 8 hits, 2 seconds.
            std::vector<float> x ((std::size_t) (2.0 * sr), 0.0f);
            const int n = (int) x.size();
            const int step = n / 8;
            for (int h = 0; h < 8; ++h)
            {
                if (h % 4 == 0)       addSineHit (x, h * step, 0.95f, 0.09, 55.0);    // kick
                else if (h % 4 == 2) { addSineHit (x, h * step, 0.50f, 0.06, 190.0);  // snare body
                                       addHit     (x, h * step, 0.60f, 0.06); }       //  + noise
                else                  addHit     (x, h * step, 0.32f, 0.012);         // hat
            }

            const auto onsets = Slicer::detectOnsets (x.data(), n, sr);
            expectEquals ((int) onsets.size(), 8);

            for (std::size_t i = 0; i < onsets.size(); ++i)
                expect (std::abs (onsets[i] - (int) i * step) < (int) (0.012 * sr),
                        "onset " + juce::String ((int) i) + " at " + juce::String (onsets[i])
                            + ", expected ~" + juce::String ((int) i * step));

            // ... and the pattern it lays out is the eighth-note grid you'd expect.
            const auto slices = Slicer::sliceToFractions (x.data(), n, sr, 16);
            const auto steps  = Slicer::slicesToSteps (slices, 16);
            expectEquals ((int) steps.size(), 8);
            for (int i = 0; i < 8; ++i)
                expectEquals (steps[(std::size_t) i], i * 2);
        }

        beginTest ("quiet ghost notes are found (the follower releases between hits)");
        {
            // Loud hits on the quarters, ghost notes at 25% amplitude between them.
            std::vector<float> x ((std::size_t) (2.0 * sr), 0.0f);
            const int n = (int) x.size();
            for (int h = 0; h < 4; ++h)
            {
                addHit (x, h * n / 4, 0.9f, 0.05);
                addHit (x, h * n / 4 + n / 8, 0.22f, 0.03);   // ghost
            }

            const auto onsets = Slicer::detectOnsets (x.data(), n, sr);
            // A fixed "30% of peak" gate finds only the 4 loud hits; we want all 8.
            expect ((int) onsets.size() >= 7,
                    "expected ~8 onsets incl. ghosts, got " + juce::String ((int) onsets.size()));
        }

        beginTest ("slices tile [0,1] contiguously with no gaps or overlaps");
        {
            const auto loop = makeLoop (6, 2.0);
            const auto slices = Slicer::sliceToFractions (loop.data(), (int) loop.size(), sr, 16);

            expectEquals ((int) slices.size(), 6);
            checkTiling (*this, slices);
        }

        beginTest ("a one-shot (or silence) falls back to an even division into maxSlices");
        {
            std::vector<float> oneShot ((std::size_t) (0.5 * sr), 0.0f);
            addHit (oneShot, 0, 0.9f, 0.2);

            const auto slices = Slicer::sliceToFractions (oneShot.data(), (int) oneShot.size(), sr, 16);
            expectEquals ((int) slices.size(), 16);
            checkTiling (*this, slices);
            expectWithinAbsoluteError (slices[1].startFraction, 1.0f / 16.0f, 1.0e-5f);

            std::vector<float> silence ((std::size_t) sr, 0.0f);
            const auto evenSlices = Slicer::sliceToFractions (silence.data(), (int) silence.size(), sr, 4);
            expectEquals ((int) evenSlices.size(), 4);
            checkTiling (*this, evenSlices);
        }

        beginTest ("more onsets than pads keeps the strongest, still tiling and ascending");
        {
            // 24 hits, alternating loud and quiet: thinning to 16 must drop quiet ones.
            std::vector<float> x ((std::size_t) (3.0 * sr), 0.0f);
            const int spacing = (int) x.size() / 24;
            for (int h = 0; h < 24; ++h)
                addHit (x, h * spacing, (h % 3 == 0) ? 0.9f : 0.3f, 0.03);

            const auto slices = Slicer::sliceToFractions (x.data(), (int) x.size(), sr, 16);
            expectEquals ((int) slices.size(), 16);
            checkTiling (*this, slices);
        }

        beginTest ("maxSlices of 1 yields the whole sample");
        {
            const auto loop = makeLoop (8, 2.0);
            const auto slices = Slicer::sliceToFractions (loop.data(), (int) loop.size(), sr, 1);
            expectEquals ((int) slices.size(), 1);
            checkTiling (*this, slices);
        }

        beginTest ("slicesToSteps places even slices on even steps, monotonically");
        {
            std::vector<Slice> even;
            for (int i = 0; i < 4; ++i)
                even.push_back ({ (float) i / 4.0f, (float) (i + 1) / 4.0f });

            const auto steps = Slicer::slicesToSteps (even, 16);
            expectEquals ((int) steps.size(), 4);
            expectEquals (steps[0], 0);
            expectEquals (steps[1], 4);
            expectEquals (steps[2], 8);
            expectEquals (steps[3], 12);
        }

        beginTest ("slicesToSteps stays in range and never goes backwards");
        {
            const auto loop = makeLoop (13, 2.0);   // 13 doesn't divide 16 evenly
            const auto slices = Slicer::sliceToFractions (loop.data(), (int) loop.size(), sr, 16);
            const auto steps  = Slicer::slicesToSteps (slices, 16);

            expectEquals ((int) steps.size(), (int) slices.size());
            for (std::size_t i = 0; i < steps.size(); ++i)
            {
                expect (steps[i] >= 0 && steps[i] < 16, "step in range");
                if (i > 0)
                    expect (steps[i] >= steps[i - 1], "steps never go backwards");
            }
            expectEquals (steps.front(), 0);
        }

        beginTest ("slicesToSteps is safe on empty input / zero steps");
        {
            expect (Slicer::slicesToSteps ({}, 16).empty());
            expect (Slicer::slicesToSteps ({ { 0.0f, 1.0f } }, 0).empty());
        }
    }
};

static SlicerTest slicerTest;

} // namespace rollforge::tests
