// RollForge — BeatboxDetector tests.
//
// Headless. The audio is synthesised to sound like the three things a mouth makes: a boomy
// "b", a noisy "p", a bright "ts". Two things must hold, and neither is worth much alone:
//
//   * WHAT: each hit is classified onto the right pad.
//   * WHEN: each hit is found within a few milliseconds of where it was put.
//
// A detector that hears three kicks in perfect time is as useless as one that hears the right
// drums in the wrong bar.

#include "library/BeatboxDetector.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double sr = 44100.0;

    int at (double seconds) { return (int) std::llround (seconds * sr); }

    /** A boomy "b": a low sine with a quick pitch drop and a long-ish body. */
    void addKick (std::vector<float>& out, double when, float gain = 1.0f)
    {
        const int start = at (when);
        const int n     = at (0.20);
        juce::Random random (7);
        juce::ignoreUnused (random);

        for (int i = 0; i < n && start + i < (int) out.size(); ++i)
        {
            const double t     = (double) i / sr;
            const double pitch = 55.0 * (1.0 + 2.5 * std::exp (-t / 0.02));
            const double env   = std::exp (-t / 0.09);
            out[(std::size_t) (start + i)] += gain * (float) (std::sin (2.0 * M_PI * pitch * t) * env);
        }
    }

    /** A noisy "p": mid-bright noise over a 190 Hz body. */
    void addSnare (std::vector<float>& out, double when, float gain = 1.0f)
    {
        const int start = at (when);
        const int n     = at (0.13);
        juce::Random random (11 + start);
        float lowpass = 0.0f;

        for (int i = 0; i < n && start + i < (int) out.size(); ++i)
        {
            const double t   = (double) i / sr;
            const double env = std::exp (-t / 0.05);
            const float  w   = random.nextFloat() * 2.0f - 1.0f;
            lowpass += (w - lowpass) * 0.12f;    // damp it: mid brightness, not a hat
            const double body = 0.55 * std::sin (2.0 * M_PI * 190.0 * t) * std::exp (-t / 0.035);
            out[(std::size_t) (start + i)] += gain * (float) ((lowpass * 1.6 + body) * env);
        }
    }

    /** A bright "ts": short, undamped noise. */
    void addHat (std::vector<float>& out, double when, float gain = 1.0f)
    {
        const int start = at (when);
        const int n     = at (0.035);
        juce::Random random (23 + start);

        for (int i = 0; i < n && start + i < (int) out.size(); ++i)
        {
            const double t   = (double) i / sr;
            const double env = std::exp (-t / 0.008);
            out[(std::size_t) (start + i)] += gain * (float) ((random.nextFloat() * 2.0f - 1.0f) * env);
        }
    }

    /** Index of the hit nearest `when`, or -1 if none is within `toleranceMs`. */
    int hitNear (const std::vector<Capture::Hit>& hits, double when, double toleranceMs = 12.0)
    {
        int best = -1;
        double bestDelta = toleranceMs / 1000.0;
        for (int i = 0; i < (int) hits.size(); ++i)
        {
            const double d = std::abs (hits[(std::size_t) i].seconds - when);
            if (d <= bestDelta) { bestDelta = d; best = i; }
        }
        return best;
    }
}

class BeatboxDetectorTest final : public juce::UnitTest
{
public:
    BeatboxDetectorTest() : juce::UnitTest ("RollForge BeatboxDetector", testCategory) {}

    void runTest() override
    {
        beginTest ("silence, emptiness and nonsense produce no hits, not phantom ones");
        {
            expect (BeatboxDetector::detect (nullptr, 1000, sr).empty());

            std::vector<float> empty;
            expect (BeatboxDetector::detect (empty.data(), 0, sr).empty());

            std::vector<float> silence ((std::size_t) at (2.0), 0.0f);
            expect (BeatboxDetector::detect (silence.data(), (int) silence.size(), sr).empty(),
                    "silence produced a hit");

            std::vector<float> kick ((std::size_t) at (1.0), 0.0f);
            addKick (kick, 0.1);
            expect (BeatboxDetector::detect (kick.data(), (int) kick.size(), 0.0).empty(),
                    "a zero sample rate must not divide by itself");
        }

        beginTest ("a single sound of each kind lands on its own pad");
        {
            struct Case { const char* name; void (*add) (std::vector<float>&, double, float); int pad; };
            const Case cases[] = {
                { "kick",  addKick,  0 },
                { "snare", addSnare, 1 },
                { "hat",   addHat,   2 },
            };

            for (const auto& c : cases)
            {
                std::vector<float> audio ((std::size_t) at (0.6), 0.0f);
                c.add (audio, 0.05, 1.0f);

                const auto hits = BeatboxDetector::detect (audio.data(), (int) audio.size(), sr);
                expectEquals ((int) hits.size(), 1, juce::String (c.name) + ": wrong number of hits");
                if (hits.size() == 1)
                    expectEquals (hits[0].pad, c.pad, juce::String (c.name) + " landed on the wrong pad");
            }
        }

        beginTest ("a bar of \"b - ts - p - ts\" comes back as the bar that was played");
        {
            // The pattern a person actually beatboxes, at 120 BPM: kick on 1, snare on 3, hats
            // on the offbeats. Two things are asserted -- what, and when.
            std::vector<float> audio ((std::size_t) at (2.2), 0.0f);

            const double kick1 = 0.00, hat1 = 0.50, snare = 1.00, hat2 = 1.50;
            addKick  (audio, kick1);
            addHat   (audio, hat1, 0.7f);
            addSnare (audio, snare);
            addHat   (audio, hat2, 0.7f);

            const auto hits = BeatboxDetector::detect (audio.data(), (int) audio.size(), sr);
            expectEquals ((int) hits.size(), 4, "wrong number of hits found");

            const int i0 = hitNear (hits, kick1);
            const int i1 = hitNear (hits, hat1);
            const int i2 = hitNear (hits, snare);
            const int i3 = hitNear (hits, hat2);

            expect (i0 >= 0, "the kick was not found within 12 ms of where it was played");
            expect (i1 >= 0, "the first hat was not found");
            expect (i2 >= 0, "the snare was not found");
            expect (i3 >= 0, "the second hat was not found");

            if (i0 >= 0) expectEquals (hits[(std::size_t) i0].pad, 0, "the kick was not a kick");
            if (i1 >= 0) expectEquals (hits[(std::size_t) i1].pad, 2, "the first hat was not a hat");
            if (i2 >= 0) expectEquals (hits[(std::size_t) i2].pad, 1, "the snare was not a snare");
            if (i3 >= 0) expectEquals (hits[(std::size_t) i3].pad, 2, "the second hat was not a hat");

            // ...and the times are ascending, because Capture::apply relies on nothing else.
            for (std::size_t i = 1; i < hits.size(); ++i)
                expect (hits[i].seconds > hits[i - 1].seconds, "hits came back out of order");
        }

        beginTest ("silence before the first hit does not become a drum on the downbeat");
        {
            // Slicer always reports an onset at sample 0. Nobody starts recording exactly on
            // their first hit, so without a silence gate every take opens with a phantom drum.
            std::vector<float> audio ((std::size_t) at (1.5), 0.0f);
            addKick (audio, 0.40);

            const auto hits = BeatboxDetector::detect (audio.data(), (int) audio.size(), sr);
            expectEquals ((int) hits.size(), 1, "a phantom hit was reported before the kick");
            if (! hits.empty())
                expect (std::abs (hits[0].seconds - 0.40) < 0.012, "the kick moved");
        }

        beginTest ("how hard it was hit becomes how hard it plays");
        {
            std::vector<float> audio ((std::size_t) at (1.6), 0.0f);
            addKick (audio, 0.10, 1.0f);    // the loudest thing here
            addKick (audio, 0.70, 0.35f);   // a ghost

            const auto hits = BeatboxDetector::detect (audio.data(), (int) audio.size(), sr);
            expectEquals ((int) hits.size(), 2);
            if (hits.size() == 2)
            {
                expectWithinAbsoluteError (hits[0].velocity, 1.0f, 0.05f, "the loudest hit is full velocity");
                expect (hits[1].velocity < 0.55f, "the ghost note came back at full strength");
                expect (hits[1].velocity > 0.10f, "the ghost note was lost");
            }
        }

        beginTest ("the pads the three classes land on are the caller's choice");
        {
            std::vector<float> audio ((std::size_t) at (0.6), 0.0f);
            addHat (audio, 0.05);

            BeatboxDetector::Options options;
            options.hatPad = 9;

            const auto hits = BeatboxDetector::detect (audio.data(), (int) audio.size(), sr, options);
            expectEquals ((int) hits.size(), 1);
            if (! hits.empty())
                expectEquals (hits[0].pad, 9);
        }
    }
};

static BeatboxDetectorTest beatboxDetectorTest;

} // namespace rollforge::tests
