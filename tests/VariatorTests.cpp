// RollForge — Variator determinism + musical-guardrail tests.
//
// Headless: same pattern+amount+seed -> identical mutation and identical change
// list; the change list is exactly the net-toggled set; reported cells really
// toggled; ghost taps are clean single hits (no inherited ratchets/probability);
// anchor hits (downbeat kick, snare backbeats) survive; locked lanes are
// untouched; an empty pattern stays silent.

#include "model/Variator.h"
#include "model/FillEngine.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <set>
#include <utility>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    std::array<bool, (std::size_t) maxLanes> noLocks() { return {}; }

    bool samePattern (const Pattern& a, const Pattern& b)
    {
        if (a.numLanes != b.numLanes)
            return false;
        for (int li = 0; li < a.numLanes; ++li)
            for (int s = 0; s < maxStepsPerLane; ++s)
            {
                const Step& sa = a.lane (li).step (s);
                const Step& sb = b.lane (li).step (s);
                if (sa.on != sb.on || std::abs (sa.velocity - sb.velocity) > 1.0e-6f)
                    return false;
            }
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

class VariatorTest final : public juce::UnitTest
{
public:
    VariatorTest() : juce::UnitTest ("RollForge Variator", testCategory) {}

    void runTest() override
    {
        beginTest ("same pattern + amount + seed -> identical mutation + change list");
        {
            Pattern base; FillEngine::generateFill (base, FillEngine::Trap, 3, 42);
            Pattern a = base, b = base;
            const auto ca = Variator::vary (a, 0.6f, 777, noLocks());
            const auto cb = Variator::vary (b, 0.6f, 777, noLocks());
            expect (samePattern (a, b));
            expectEquals ((int) ca.size(), (int) cb.size());
            for (size_t i = 0; i < ca.size() && i < cb.size(); ++i)
                expect (ca[i].lane == cb[i].lane && ca[i].step == cb[i].step);
        }

        beginTest ("every reported cell actually toggled");
        {
            Pattern base; FillEngine::generateFill (base, FillEngine::BoomBap, 4, 5);
            Pattern after = base;
            const auto changes = Variator::vary (after, 0.8f, 9001, noLocks());
            expect (! changes.empty());   // a non-empty groove should always change something
            for (const auto& c : changes)
            {
                const bool wasOn = base.lane (c.lane).step (c.step).on;
                const bool nowOn = after.lane (c.lane).step (c.step).on;
                expect (wasOn != nowOn);
            }
        }

        beginTest ("anchor hits survive repeated varies");
        {
            // Boom Bap uses a snare backbeat (Trap/House use a clap backbeat), so it
            // exercises both the kick and snare anchor guards.
            Pattern p; FillEngine::generateFill (p, FillEngine::BoomBap, 5, 3);
            expect (p.lane (0).step (0).on);    // kick downbeat
            expect (p.lane (1).step (4).on);    // snare backbeats
            expect (p.lane (1).step (12).on);
            for (std::uint64_t s = 1; s <= 40; ++s)
                Variator::vary (p, 1.0f, s, noLocks());
            expect (p.lane (0).step (0).on);
            expect (p.lane (1).step (4).on);
            expect (p.lane (1).step (12).on);
        }

        beginTest ("a locked lane is left untouched");
        {
            Pattern base; FillEngine::generateFill (base, FillEngine::House, 4, 88);
            Pattern after = base;
            auto locks = noLocks();
            locks[0] = true;   // lock the kick lane
            Variator::vary (after, 1.0f, 1234, locks);

            for (int s = 0; s < maxStepsPerLane; ++s)
            {
                expect (after.lane (0).step (s).on == base.lane (0).step (s).on);
                expect (std::abs (after.lane (0).step (s).velocity
                                   - base.lane (0).step (s).velocity) < 1.0e-6f);
            }
        }

        beginTest ("an empty pattern stays silent");
        {
            Pattern empty;
            empty.numLanes = maxLanes;
            for (int li = 0; li < maxLanes; ++li)
            {
                empty.lane (li).targetPad = li;
                empty.lane (li).length    = 16;
            }
            const auto changes = Variator::vary (empty, 1.0f, 55, noLocks());
            expect (changes.empty());
            expectEquals (onSteps (empty), 0);
        }

        beginTest ("different seeds usually differ");
        {
            Pattern base; FillEngine::generateFill (base, FillEngine::Drill, 3, 2);
            Pattern a = base, b = base;
            Variator::vary (a, 0.7f, 1, noLocks());
            Variator::vary (b, 0.7f, 987654, noLocks());
            expect (! samePattern (a, b));
        }

        beginTest ("Vary's ghost taps are clean single hits (no inherited ratchets/probability)");
        {
            // A lane of ratcheted, low-probability, sample-locked off-beat hits at
            // velocity 1.0: Vary drops some and re-ghosts empty steps. A re-enabled
            // "ghost" must be a plain single hit — NOT inherit a dropped step's 3x
            // ratchet (which would play as a buzz). With 2 attempts/lane (amount 0.5)
            // an original never falls below ~0.6, so any post-vary on-step under the
            // ghost ceiling (~0.37) is necessarily a ghost.
            for (std::uint64_t seed = 0; seed < 500; ++seed)
            {
                Pattern p;
                p.numLanes = 1;
                p.lane (0).targetPad = 6;    // a non-anchor pad (toms): every hit is removable
                p.lane (0).length    = 16;
                for (int s = 1; s < 16; s += 2)   // off-beat 16ths -> ghost-eligible once dropped
                {
                    Step& st       = p.lane (0).step (s);
                    st.on          = true;
                    st.velocity    = 1.0f;
                    st.ratchets    = 3;
                    st.probability = 50;
                    st.microShift  = 0.25f;
                    st.sampleLock  = 4;
                }

                Variator::vary (p, 0.5f, seed, noLocks());

                for (int s = 0; s < 16; ++s)
                {
                    const Step& st = p.lane (0).step (s);
                    if (st.on && st.velocity < 0.45f)   // a ghost (originals stay >= ~0.6 here)
                    {
                        expectEquals (st.ratchets, 1);
                        expectEquals (st.probability, 100);
                        expect (std::abs (st.microShift) < 1.0e-6f);
                        expectEquals (st.sampleLock, -1);
                    }
                }
            }
        }

        beginTest ("the change list is exactly the set of net-toggled cells");
        {
            Pattern base; FillEngine::generateFill (base, FillEngine::Trap, 4, 314);
            Pattern after = base;
            const auto changes = Variator::vary (after, 0.9f, 271828, noLocks());

            std::set<std::pair<int, int>> reported;
            for (const auto& c : changes)
                expect (reported.insert ({ c.lane, c.step }).second);   // no duplicate cells

            for (int li = 0; li < after.numLanes; ++li)
                for (int s = 0; s < maxStepsPerLane; ++s)
                {
                    const bool toggled = base.lane (li).step (s).on != after.lane (li).step (s).on;
                    expect (toggled == (reported.count ({ li, s }) > 0));   // reported <=> toggled
                }
        }
    }
};

static VariatorTest variatorTest;

} // namespace rollforge::tests
