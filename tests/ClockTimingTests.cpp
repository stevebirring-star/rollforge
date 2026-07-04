// RollForge — Clock sample-accurate timing tests (Phase 2, commit 2).
//
// The acceptance gate for sequencer timing: exact step positions, and IDENTICAL
// absolute step positions regardless of audio buffer size (drift-free), plus
// clean tempo changes and transport behaviour.

#include "engine/Clock.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    struct Fired
    {
        std::int64_t index;
        std::int64_t absoluteSample;
    };

    // Runs `clock` for `totalSamples` in fixed `bufferSize` blocks, collecting the
    // absolute sample position of every fired step.
    std::vector<Fired> run (Clock& clock, int bufferSize, int totalSamples)
    {
        std::vector<Fired> fired;
        int consumed = 0;
        while (consumed < totalSamples)
        {
            const int n = juce::jmin (bufferSize, totalSamples - consumed);
            const std::int64_t blockStart = clock.getSampleCounter();
            clock.processBlock (n, [&] (std::int64_t index, int offset)
            {
                fired.push_back ({ index, blockStart + (std::int64_t) offset });
            });
            consumed += n;
        }
        return fired;
    }
}

class ClockTimingTest final : public juce::UnitTest
{
public:
    ClockTimingTest() : juce::UnitTest ("RollForge ClockTiming", testCategory) {}

    void runTest() override
    {
        beginTest ("exact 1/16 step positions at 120 BPM / 44100 Hz");
        {
            Clock clock;
            clock.prepare (44100.0);
            clock.setTempo (120.0);
            clock.setPlaying (true);

            // samplesPerStep = 44100*60/(120*4) = 5512.5; one second = 8 steps.
            const auto fired = run (clock, 44100, 44100);
            expectEquals ((int) fired.size(), 8);
            for (int n = 0; n < (int) fired.size(); ++n)
            {
                expectEquals (fired[(size_t) n].index, (std::int64_t) n);
                expectEquals (fired[(size_t) n].absoluteSample, (std::int64_t) std::llround (n * 5512.5));
            }
        }

        beginTest ("step positions are identical across buffer sizes (drift-free)");
        {
            auto positionsAt = [] (int bufferSize)
            {
                Clock clock;
                clock.prepare (48000.0);
                clock.setTempo (137.0);       // deliberately non-round tempo/rate
                clock.setPlaying (true);
                return run (clock, bufferSize, 48000 * 4);   // 4 seconds
            };

            const auto a = positionsAt (64);
            const auto b = positionsAt (256);
            const auto c = positionsAt (1024);
            const auto d = positionsAt (1000);   // non-power-of-two too

            expect (! a.empty());
            expectEquals ((int) b.size(), (int) a.size());
            expectEquals ((int) c.size(), (int) a.size());
            expectEquals ((int) d.size(), (int) a.size());

            for (size_t i = 0; i < a.size(); ++i)
            {
                expectEquals (b[i].index, a[i].index);
                expectEquals (b[i].absoluteSample, a[i].absoluteSample);
                expectEquals (c[i].absoluteSample, a[i].absoluteSample);
                expectEquals (d[i].absoluteSample, a[i].absoluteSample);
            }
        }

        beginTest ("consecutive step spacing stays within one sample of ideal");
        {
            Clock clock;
            clock.prepare (44100.0);
            clock.setTempo (128.0);
            clock.setPlaying (true);

            const auto fired = run (clock, 128, 44100 * 2);
            const double ideal = 44100.0 * 60.0 / (128.0 * 4.0);
            for (size_t i = 1; i < fired.size(); ++i)
            {
                const auto gap = fired[i].absoluteSample - fired[i - 1].absoluteSample;
                expect (std::abs ((double) gap - ideal) <= 1.0);   // rounding only
            }
        }

        beginTest ("a tempo change respaces later steps without moving the pending one");
        {
            Clock clock;
            clock.prepare (44100.0);
            clock.setTempo (120.0);
            clock.setPlaying (true);

            std::vector<Fired> fired;
            auto block = [&] (int n)
            {
                const std::int64_t start = clock.getSampleCounter();
                clock.processBlock (n, [&] (std::int64_t idx, int off)
                                        { fired.push_back ({ idx, start + off }); });
            };

            block (10000);            // ~1 step at 5512.5 (steps 0,1)
            clock.setTempo (240.0);   // double tempo -> samplesPerStep 2756.25
            block (10000);

            expect (fired.size() >= 4);
            // Steps before the change keep 120-BPM spacing; after, ~240 BPM.
            expectEquals (fired[0].absoluteSample, (std::int64_t) 0);
            expectEquals (fired[1].absoluteSample, (std::int64_t) std::llround (5512.5));
            // After the rebase, the gap between later steps is the faster spacing.
            const auto lateGap = fired.back().absoluteSample - fired[fired.size() - 2].absoluteSample;
            expect (std::abs ((double) lateGap - 2756.25) <= 1.0);
        }

        beginTest ("no steps fire while stopped; reset rewinds to step 0");
        {
            Clock clock;
            clock.prepare (44100.0);
            clock.setTempo (120.0);

            clock.setPlaying (false);
            auto none = run (clock, 512, 44100);
            expect (none.empty());

            clock.setPlaying (true);
            auto first = run (clock, 512, 6000);
            expect (! first.empty());
            expectEquals (first.front().index, (std::int64_t) 0);

            clock.reset();
            auto again = run (clock, 512, 6000);
            expect (! again.empty());
            expectEquals (again.front().index, (std::int64_t) 0);          // rewound
            expectEquals (again.front().absoluteSample, (std::int64_t) 0);
        }
    }
};

static ClockTimingTest clockTimingTest;

} // namespace rollforge::tests
