// RollForge — RollCompiler unit tests (Phase 3).
//
// Headless checks of the pure roll compiler: even step spacing at constant speed,
// tightening at accelerating speed, velocity/pitch ramps, determinism, degenerate
// cases, and the CompiledRoll wrapper. Offsets are in STEPS (tempo-independent).

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

        beginTest ("constant-speed roll places evenly-spaced hits (in steps)");
        {
            RollRegion r;
            r.lengthSteps = 1.0;
            r.speed  = { 4.0f, 4.0f, 0.0f };
            r.volume = { 1.0f, 1.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r);
            expectEquals ((int) ev.size(), 4);
            expectWithinAbsoluteError (ev[0].stepOffset, 0.00f, 0.001f);
            expectWithinAbsoluteError (ev[1].stepOffset, 0.25f, 0.001f);
            expectWithinAbsoluteError (ev[2].stepOffset, 0.50f, 0.001f);
            expectWithinAbsoluteError (ev[3].stepOffset, 0.75f, 0.001f);
        }

        beginTest ("accelerating roll packs hits tighter over time");
        {
            RollRegion r;
            r.lengthSteps = 2.0;
            r.speed = { 2.0f, 8.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r);
            expect (ev.size() >= 6);

            for (size_t i = 1; i < ev.size(); ++i)
                expect (ev[i].stepOffset > ev[i - 1].stepOffset);   // monotonic

            for (size_t i = 2; i < ev.size(); ++i)
            {
                const float gapPrev = ev[i - 1].stepOffset - ev[i - 2].stepOffset;
                const float gapCurr = ev[i].stepOffset - ev[i - 1].stepOffset;
                expect (gapCurr <= gapPrev + 1.0e-3f);   // non-increasing
            }
        }

        beginTest ("volume + pitch curves ramp the hits");
        {
            RollRegion r;
            r.lengthSteps = 1.0;
            r.speed  = { 8.0f, 8.0f, 0.0f };
            r.volume = { 1.0f, 0.2f, 0.0f };
            r.pitch  = { 0.0f, 12.0f, 0.0f };

            const auto ev = RollCompiler::compileRoll (r);
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

            const auto a = RollCompiler::compileRoll (r);
            const auto b = RollCompiler::compileRoll (r);
            expectEquals ((int) a.size(), (int) b.size());
            for (size_t i = 0; i < a.size(); ++i)
            {
                expectWithinAbsoluteError (a[i].stepOffset, b[i].stepOffset, 1.0e-6f);
                expectWithinAbsoluteError (a[i].velocity, b[i].velocity, 1.0e-6f);
            }
        }

        beginTest ("compile() fills a CompiledRoll; zero-length rolls are empty");
        {
            RollRegion r;
            r.startStep = 3.0; r.lengthSteps = 1.0; r.targetPad = 5;
            r.speed = { 4.0f, 4.0f, 0.0f };

            const CompiledRoll cr = RollCompiler::compile (r);
            expectEquals (cr.targetPad, 5);
            expectWithinAbsoluteError (cr.startStep, 3.0f, 1.0e-4f);
            expectEquals (cr.count, 4);

            RollRegion zeroLength; zeroLength.lengthSteps = 0.0;
            expect (RollCompiler::compileRoll (zeroLength).empty());
            expectEquals (RollCompiler::compile (zeroLength).count, 0);
        }

        beginTest ("denser roll compiles to more hits — the brush meter's 'Hits ~N' contract");
        {
            // Mirrors MainComponent::buildBrushRegion's Auto mapping: end speed =
            // 2 + density*14, so density 0 -> 2 and density 1 -> 16 over one span.
            RollRegion sparse;
            sparse.lengthSteps = 4.0;
            sparse.speed = { 2.0f, 2.0f, 0.3f };    // density 0

            RollRegion dense = sparse;
            dense.speed = { 2.0f, 16.0f, 0.3f };    // density 1

            const int sparseHits = RollCompiler::compile (sparse).count;
            const int denseHits  = RollCompiler::compile (dense).count;
            expect (sparseHits > 0);
            expect (denseHits > sparseHits);        // the meter's hit count must rise with density
        }
    }
};

static RollCompilerTest rollCompilerTest;

} // namespace rollforge::tests
