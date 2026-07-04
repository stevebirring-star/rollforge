// RollForge — RollCompiler unit tests (Phase 3, commit 1).
//
// Headless checks of the pure roll compiler: even spacing at constant speed,
// tightening at accelerating speed, velocity/pitch ramps, determinism, and edge
// cases. This is the heavily-tested core the roll UI and presets wrap.

#include "model/RollCompiler.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class RollCompilerTest final : public juce::UnitTest
{
public:
    RollCompilerTest() : juce::UnitTest ("RollForge RollCompiler", testCategory) {}

    void runTest() override
    {
        beginTest ("curveValue eases start -> end");
        {
            const RollCurve linear { 0.0f, 10.0f, 0.0f };
            expectWithinAbsoluteError (RollCompiler::curveValue (linear, 0.0f), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (RollCompiler::curveValue (linear, 1.0f), 10.0f, 1.0e-4f);
            expectWithinAbsoluteError (RollCompiler::curveValue (linear, 0.5f), 5.0f, 1.0e-4f);

            const RollCurve accel { 0.0f, 10.0f, 1.0f };   // slow start
            expect (RollCompiler::curveValue (accel, 0.5f) < 5.0f);
        }

        beginTest ("constant-speed roll places evenly-spaced hits");
        {
            RollRegion r;
            r.lengthSteps = 1.0;
            r.speed  = { 4.0f, 4.0f, 0.0f };
            r.volume = { 1.0f, 1.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r, 4800.0);
            expectEquals ((int) ev.size(), 4);
            expectEquals (ev[0].sampleOffset, 0);
            // ~1200-sample spacing (allow a couple samples for FP accumulation).
            expect (ev[1].sampleOffset >= 1198 && ev[1].sampleOffset <= 1202);
            expect (ev[2].sampleOffset >= 2398 && ev[2].sampleOffset <= 2402);
            expect (ev[3].sampleOffset >= 3598 && ev[3].sampleOffset <= 3602);
        }

        beginTest ("accelerating roll packs hits tighter over time");
        {
            RollRegion r;
            r.lengthSteps = 2.0;
            r.speed = { 2.0f, 8.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r, 4800.0);
            expect (ev.size() >= 6);

            for (size_t i = 1; i < ev.size(); ++i)
                expect (ev[i].sampleOffset > ev[i - 1].sampleOffset);   // monotonic

            for (size_t i = 2; i < ev.size(); ++i)
            {
                const int gapPrev = ev[i - 1].sampleOffset - ev[i - 2].sampleOffset;
                const int gapCurr = ev[i].sampleOffset - ev[i - 1].sampleOffset;
                expect (gapCurr <= gapPrev + 1);   // non-increasing (+1 for rounding)
            }
        }

        beginTest ("volume + pitch curves ramp the hits");
        {
            RollRegion r;
            r.lengthSteps = 1.0;
            r.speed  = { 8.0f, 8.0f, 0.0f };
            r.volume = { 1.0f, 0.2f, 0.0f };
            r.pitch  = { 0.0f, 12.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r, 4800.0);
            expect (ev.size() >= 4);
            expectWithinAbsoluteError (ev.front().velocity, 1.0f, 0.05f);
            expect (ev.back().velocity < ev.front().velocity);
            expectWithinAbsoluteError (ev.front().pitchSemitones, 0.0f, 0.5f);
            expect (ev.back().pitchSemitones > ev.front().pitchSemitones);
        }

        beginTest ("compilation is deterministic");
        {
            RollRegion r;
            r.lengthSteps = 1.5;
            r.speed  = { 3.0f, 9.0f, 0.5f };
            r.volume = { 1.0f, 0.4f, 0.0f };
            r.pitch  = { 0.0f, 7.0f, 0.0f };

            const auto a = RollCompiler::compileRoll (r, 5512.5);
            const auto b = RollCompiler::compileRoll (r, 5512.5);
            expectEquals ((int) a.size(), (int) b.size());
            for (size_t i = 0; i < a.size(); ++i)
            {
                expectEquals (a[i].sampleOffset, b[i].sampleOffset);
                expectWithinAbsoluteError (a[i].velocity, b[i].velocity, 1.0e-6f);
            }
        }

        beginTest ("degenerate rolls produce no events");
        {
            RollRegion zeroLength; zeroLength.lengthSteps = 0.0;
            expect (RollCompiler::compileRoll (zeroLength, 4800.0).empty());

            RollRegion normal; normal.lengthSteps = 1.0;
            expect (RollCompiler::compileRoll (normal, 0.0).empty());   // no samplesPerStep
        }
    }
};

static RollCompilerTest rollCompilerTest;

} // namespace rollforge::tests
