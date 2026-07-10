#include "model/Variator.h"

namespace rollforge
{
namespace Variator
{

namespace
{
    // Deterministic PRNG (SplitMix64) — reproducible per seed. (Same generator the
    // FillEngine uses; kept local so the two modules stay independent.)
    struct SplitMix64
    {
        std::uint64_t state;
        explicit SplitMix64 (std::uint64_t seed) : state (seed) {}

        std::uint64_t next() noexcept
        {
            state += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }

        float unit() noexcept { return (float) ((double) (next() >> 40) / (double) (1u << 24)); }  // [0,1)
        int   range (int lo, int hi) noexcept { return lo + (int) (unit() * (float) (hi - lo + 1)); }
    };

    // Pad indices (match the StarterKit / FillEngine layout).
    constexpr int Kick = 0, Snare = 1, Clap = 4;

    int laneLength (const Lane& lane) noexcept
    {
        return lane.length < 1 ? 1 : (lane.length > maxStepsPerLane ? maxStepsPerLane : lane.length);
    }

    // Place a clean "ghost" tap. A re-enabled step must NOT inherit stale ratchets /
    // probability / micro-shift / sample-lock from whatever hit used to live there
    // (a dropped step keeps its fields), or a quiet ghost can play as a buzzing
    // ratchet roll. Reset the whole step to a plain single hit, then set the ghost.
    void placeGhost (Step& st, float velocity) noexcept
    {
        st          = Step {};      // defaults: ratchets 1, probability 100, shift 0, lock -1
        st.on       = true;
        st.velocity = velocity;
    }
}

std::vector<Change> vary (Pattern& pattern,
                          float amount,
                          std::uint64_t seed,
                          const std::array<bool, (std::size_t) maxLanes>& lockedLanes)
{
    amount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    SplitMix64 rng (seed ^ 0xD1B54A32D192ED03ull);   // decorrelate from FillEngine's seed use

    const int lanes = pattern.numLanes < maxLanes ? pattern.numLanes : maxLanes;

    // Snapshot the on-states up front. Changes are reported as the NET diff against
    // this snapshot, so a step toggled twice within one vary (added then dropped)
    // correctly reports as no change, and the returned list is naturally deduped.
    bool before[(std::size_t) maxLanes][(std::size_t) maxStepsPerLane];
    for (int li = 0; li < lanes; ++li)
        for (int s = 0; s < maxStepsPerLane; ++s)
            before[(std::size_t) li][(std::size_t) s] = pattern.lane (li).step (s).on;

    const int attemptsPerLane = 1 + (int) (amount * 2.0f);   // amount 0->1, 0.5->2, 1->3
    int totalHits = 0;

    for (int li = 0; li < lanes; ++li)
    {
        if (lockedLanes[(std::size_t) li])
            continue;

        Lane&     lane = pattern.lane (li);
        const int len  = laneLength (lane);
        const int pad  = lane.targetPad;

        int hits = 0;
        for (int s = 0; s < len; ++s)
            if (lane.step (s).on)
                ++hits;
        totalHits += hits;

        // Vary evolves the groove that's already there — it never invents a new
        // instrument out of a silent lane.
        if (hits == 0)
            continue;

        // Anchor hits keep the groove standing: the downbeat kick and the snare/clap
        // backbeats are never dropped (accents may still touch their velocity).
        auto isAnchor = [pad] (int step) noexcept
        {
            if (pad == Kick)                  return step == 0;
            if (pad == Snare || pad == Clap)  return step == 4 || step == 12;
            return false;
        };

        for (int attempt = 0; attempt < attemptsPerLane; ++attempt)
        {
            // Re-scan each attempt: an earlier attempt on this lane may have moved a hit.
            int emptyOdd[maxStepsPerLane]; int nEmptyOdd = 0;
            int emptyAny[maxStepsPerLane]; int nEmptyAny = 0;
            int removable[maxStepsPerLane]; int nRemovable = 0;
            int onAny[maxStepsPerLane];    int nOnAny = 0;
            for (int s = 0; s < len; ++s)
            {
                if (lane.step (s).on)
                {
                    onAny[nOnAny++] = s;
                    if (! isAnchor (s))
                        removable[nRemovable++] = s;
                }
                else
                {
                    emptyAny[nEmptyAny++] = s;
                    if (s % 2 == 1)                       // off-16ths read as ghost placements
                        emptyOdd[nEmptyOdd++] = s;
                }
            }

            const float pick   = rng.unit();
            const float ghostP = 0.45f, dropP = 0.30f;

            if (pick < ghostP && (nEmptyOdd > 0 || nEmptyAny > 0))
            {
                const int s = nEmptyOdd > 0 ? emptyOdd[rng.range (0, nEmptyOdd - 1)]
                                            : emptyAny[rng.range (0, nEmptyAny - 1)];
                placeGhost (lane.step (s), 0.25f + rng.unit() * 0.12f);   // clean quiet ghost
            }
            else if (pick < ghostP + dropP && nRemovable > 0)
            {
                const int s = removable[rng.range (0, nRemovable - 1)];
                lane.step (s).on = false;
            }
            else if (nOnAny > 0)
            {
                // Accent: nudge an existing hit's velocity. A feel change, not a toggle,
                // so the diff below won't (and shouldn't) report it.
                const int   s = onAny[rng.range (0, nOnAny - 1)];
                const float v = lane.step (s).velocity + (rng.unit() * 0.4f - 0.2f);
                lane.step (s).velocity = v < 0.2f ? 0.2f : (v > 1.0f ? 1.0f : v);
            }
        }
    }

    auto collectChanges = [&]
    {
        std::vector<Change> out;
        for (int li = 0; li < lanes; ++li)
        {
            if (lockedLanes[(std::size_t) li])
                continue;
            for (int s = 0; s < maxStepsPerLane; ++s)
                if (pattern.lane (li).step (s).on != before[(std::size_t) li][(std::size_t) s])
                    out.push_back ({ li, s });
        }
        return out;
    };

    std::vector<Change> changes = collectChanges();

    // Guarantee a visible result on a non-empty pattern: if every attempt happened
    // to land on accents (or cancel out), drop one ghost (prefer the snare) so Vary
    // never looks like it did nothing. A genuinely empty pattern is left silent.
    if (totalHits > 0 && changes.empty())
    {
        auto tryGhost = [&] (int li) -> bool
        {
            if (li < 0 || li >= lanes || lockedLanes[(std::size_t) li])
                return false;

            Lane&     lane = pattern.lane (li);
            const int len  = laneLength (lane);
            for (int s = 1; s < len; s += 2)             // off-beats first
                if (! lane.step (s).on)
                {
                    placeGhost (lane.step (s), 0.3f);
                    return true;
                }
            for (int s = 0; s < len; ++s)
                if (! lane.step (s).on)
                {
                    placeGhost (lane.step (s), 0.3f);
                    return true;
                }
            return false;
        };

        if (! tryGhost (Snare))
            for (int li = 0; li < lanes; ++li)
                if (tryGhost (li))
                    break;

        changes = collectChanges();
    }

    return changes;
}

} // namespace Variator
} // namespace rollforge
