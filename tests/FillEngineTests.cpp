// RollForge — FillEngine determinism + character tests (Phase 3, commit 4).
//
// Headless: same style+intensity+seed -> identical fill; every style names +
// produces a non-trivial fill; intensity adds density + a roll; different seeds
// vary the random elements.

#include "model/FillEngine.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    bool sameFill (const Pattern& a, const Pattern& b)
    {
        if (a.numLanes != b.numLanes || a.numRolls != b.numRolls)
            return false;

        for (int li = 0; li < a.numLanes; ++li)
        {
            if (a.lane (li).targetPad != b.lane (li).targetPad)
                return false;
            for (int s = 0; s < maxStepsPerLane; ++s)
            {
                const Step& sa = a.lane (li).step (s);
                const Step& sb = b.lane (li).step (s);
                if (sa.on != sb.on || std::abs (sa.velocity - sb.velocity) > 1.0e-6f)
                    return false;
            }
        }
        for (int ri = 0; ri < a.numRolls; ++ri)
            if (a.rolls[(size_t) ri].count != b.rolls[(size_t) ri].count)
                return false;
        return true;
    }

    int onSteps (const Pattern& p)
    {
        int n = 0;
        for (int li = 0; li < p.numLanes; ++li)
            for (int s = 0; s < 16; ++s)
                if (p.lane (li).step (s).on)
                    ++n;
        return n;
    }
}

class FillEngineTest final : public juce::UnitTest
{
public:
    FillEngineTest() : juce::UnitTest ("RollForge FillEngine", testCategory) {}

    void runTest() override
    {
        beginTest ("same style + intensity + seed -> identical fill");
        {
            Pattern a, b;
            FillEngine::generateFill (a, FillEngine::Trap, 3, 12345);
            FillEngine::generateFill (b, FillEngine::Trap, 3, 12345);
            expect (sameFill (a, b));
        }

        beginTest ("every style names and produces a non-trivial fill");
        {
            for (int i = 0; i < FillEngine::NumStyles; ++i)
            {
                const auto st = (FillEngine::Style) i;
                expect (juce::String (FillEngine::styleName (st)).isNotEmpty());

                Pattern p;
                FillEngine::generateFill (p, st, 3, 1);
                expect (p.numLanes > 0);
                expect (onSteps (p) > 0);
            }
        }

        beginTest ("higher intensity adds density + a roll");
        {
            Pattern lo, hi;
            FillEngine::generateFill (lo, FillEngine::House, 1, 7);
            FillEngine::generateFill (hi, FillEngine::House, 5, 7);
            expect (onSteps (hi) > onSteps (lo));
            expect (hi.numRolls >= lo.numRolls);
        }

        beginTest ("intensity >= 3 adds a roll");
        {
            Pattern lo, hi;
            FillEngine::generateFill (lo, FillEngine::Trap, 2, 5);
            FillEngine::generateFill (hi, FillEngine::Trap, 3, 5);
            expectEquals (lo.numRolls, 0);
            expectEquals (hi.numRolls, 1);
        }

        beginTest ("different seeds usually differ");
        {
            Pattern a, b;
            FillEngine::generateFill (a, FillEngine::Trap, 3, 1);
            FillEngine::generateFill (b, FillEngine::Trap, 3, 999);
            expect (! sameFill (a, b));
        }
    }
};

static FillEngineTest fillEngineTest;

} // namespace rollforge::tests
