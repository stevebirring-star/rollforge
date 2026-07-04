// RollForge — Humaniser tests (Phase 3, commit 5).
//
// Headless: robot (amount 0) is a no-op; jitter is forward/bounded/deterministic;
// scales with amount; timing vs velocity streams are independent; and applying it
// through the Sequencer preserves the hit count (it's micro, not structural).

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"
#include "model/Humaniser.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class HumaniserTest final : public juce::UnitTest
{
public:
    HumaniserTest() : juce::UnitTest ("RollForge Humaniser", testCategory) {}

    void runTest() override
    {
        beginTest ("robot (amount 0) applies no jitter");
        {
            for (std::int64_t step = 0; step < 32; ++step)
            {
                const auto h = Humaniser::eventHash (step, (int) (step % 8), 0);
                expectWithinAbsoluteError (Humaniser::timingSteps (h, 0.0f), 0.0f, 0.0f);
                expectWithinAbsoluteError (Humaniser::velocityDelta (h, 0.0f), 0.0f, 0.0f);
            }
        }

        beginTest ("jitter is forward, bounded, and deterministic");
        {
            for (std::int64_t step = 0; step < 64; ++step)
            {
                const auto h = Humaniser::eventHash (step, (int) (step % 8), 0);

                const float t = Humaniser::timingSteps (h, 1.0f);
                expect (t >= 0.0f && t <= 0.2001f);              // forward-only, bounded

                const float v = Humaniser::velocityDelta (h, 1.0f);
                expect (v >= -0.2501f && v <= 0.2501f);

                expectWithinAbsoluteError (Humaniser::timingSteps (h, 1.0f), t, 0.0f);   // deterministic
                expectWithinAbsoluteError (Humaniser::velocityDelta (h, 1.0f), v, 0.0f);
            }
        }

        beginTest ("jitter scales with amount, streams are independent");
        {
            const auto h = Humaniser::eventHash (5, 2, 0);
            expect (Humaniser::timingSteps (h, 1.0f) >= Humaniser::timingSteps (h, 0.5f));
            expect (std::abs (Humaniser::velocityDelta (h, 1.0f)) >= std::abs (Humaniser::velocityDelta (h, 0.5f)));

            expect (Humaniser::eventHash (9, 3, 0) != Humaniser::eventHash (9, 3, 1));   // timing vs velocity
        }

        beginTest ("humanise preserves the hit count through the Sequencer");
        {
            const double sr = 44100.0;

            auto countHits = [&] (float humanise)
            {
                DrumEngine engine;
                engine.prepare (sr, 512);
                Sequencer seq;
                seq.prepare (sr);
                seq.setTempo (120.0);
                seq.setHumanise (humanise);

                Pattern p;
                p.numLanes = 1;
                p.lane (0).targetPad = 0;
                p.lane (0).length = 16;
                for (int s = 0; s < 16; s += 4) { p.lane (0).step (s).on = true; p.lane (0).step (s).velocity = 0.8f; }
                seq.setPattern (p);
                seq.setPlaying (true);

                // Window covers steps 0/4/8/12 (+ their forward jitter) but stops
                // before step 16, so the count is jitter-independent.
                const int total = 80000;
                int done = 0;
                while (done < total)
                {
                    const int n = juce::jmin (512, total - done);
                    juce::AudioBuffer<float> b (1, n);
                    b.clear();
                    seq.process (engine, b);
                    done += n;
                }
                return seq.getTriggerCount();
            };

            const auto robot = countHits (0.0f);
            expect (robot > 0);
            expectEquals (countHits (1.0f), robot);
        }
    }
};

static HumaniserTest humaniserTest;

} // namespace rollforge::tests
