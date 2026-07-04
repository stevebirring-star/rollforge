#include "model/FillEngine.h"

#include "model/RollCompiler.h"
#include "model/RollPresets.h"

namespace rollforge
{
namespace FillEngine
{

namespace
{
    // Deterministic PRNG (SplitMix64) — reproducible per seed.
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
        bool  chance (float p) noexcept { return unit() < p; }
        int   range (int lo, int hi) noexcept { return lo + (int) (unit() * (float) (hi - lo + 1)); }
    };

    // Pad indices (match the StarterKit layout).
    constexpr int Kick = 0, Snare = 1, CHat = 2, LowTom = 6, MidTom = 7, HighTom = 8;

    void setupLane (Pattern& p, int laneIndex, int targetPad)
    {
        p.lane (laneIndex).targetPad = targetPad;
        p.lane (laneIndex).length    = 16;
        for (int s = 0; s < maxStepsPerLane; ++s)
            p.lane (laneIndex).step (s) = Step {};   // clear
    }

    void put (Pattern& p, int laneIndex, int step, float velocity)
    {
        if (step >= 0 && step < 16)
        {
            p.lane (laneIndex).step (step).on       = true;
            p.lane (laneIndex).step (step).velocity = velocity;
        }
    }

    RollPresets::Preset rollForStyle (Style style) noexcept
    {
        switch (style)
        {
            case Trap:    return RollPresets::MachineGun;
            case Drill:   return RollPresets::DrillSlide;
            case House:   return RollPresets::Crescendo;
            case DnB:     return RollPresets::Ramp32;
            case BoomBap: return RollPresets::FadeRoll;
            case Techno:  return RollPresets::Stutter;
            case Pop:     return RollPresets::Buildup;
            default:      return RollPresets::Buildup;
        }
    }
}

const char* styleName (Style style) noexcept
{
    switch (style)
    {
        case Trap:    return "Trap";
        case Drill:   return "Drill";
        case House:   return "House";
        case DnB:     return "DnB";
        case BoomBap: return "Boom Bap";
        case Techno:  return "Techno";
        case Pop:     return "Pop";
        default:      return "Fill";
    }
}

void generateFill (Pattern& pattern, Style style, int intensity, std::uint64_t seed)
{
    intensity = intensity < 1 ? 1 : (intensity > 5 ? 5 : intensity);
    SplitMix64 rng (seed);

    // Lanes: kick, snare, closed hat, 3 toms. (Reset in place — no big temporary.)
    const int laneKick = 0, laneSnare = 1, laneHat = 2, laneLow = 3, laneMid = 4, laneHigh = 5;
    pattern.numLanes = 6;
    pattern.numRolls = 0;
    setupLane (pattern, laneKick,  Kick);
    setupLane (pattern, laneSnare, Snare);
    setupLane (pattern, laneHat,   CHat);
    setupLane (pattern, laneLow,   LowTom);
    setupLane (pattern, laneMid,   MidTom);
    setupLane (pattern, laneHigh,  HighTom);

    // Hats: denser at higher intensity.
    const int hatEvery = 6 - intensity;                    // intensity 5 -> every 1; 1 -> every 5
    for (int s = 0; s < 16; s += (hatEvery < 1 ? 1 : hatEvery))
        put (pattern, laneHat, s, 0.55f);

    // Kick.
    if (style == House || style == Techno)
    {
        for (int s = 0; s < 16; s += 4)
            put (pattern, laneKick, s, 0.95f);             // four on the floor
    }
    else
    {
        put (pattern, laneKick, 0, 0.95f);
        if (rng.chance (0.6f))
            put (pattern, laneKick, style == Trap ? 6 : 8, 0.85f);
    }

    // Snare backbeat + ghost notes.
    put (pattern, laneSnare, 4,  0.9f);
    put (pattern, laneSnare, 12, 0.9f);
    for (int g = 0; g < intensity; ++g)
    {
        const int s = rng.range (1, 15);
        if (! pattern.lane (laneSnare).step (s).on)
            put (pattern, laneSnare, s, 0.3f);             // ghost
    }

    // Descending tom run in the last 4 steps.
    const int tomLanes[3] = { laneHigh, laneMid, laneLow };
    for (int s = 12; s < 16; ++s)
        if (rng.chance (0.4f + (float) intensity * 0.1f))
            put (pattern, tomLanes[(s - 12) % 3], s, 0.85f);

    // Snare roll at the end for intensity >= 3.
    if (intensity >= 3 && pattern.numRolls < maxRolls)
    {
        const RollRegion r = RollPresets::make (rollForStyle (style), 14.0, 2.0, Snare);
        pattern.rolls[(size_t) pattern.numRolls] = RollCompiler::compile (r);
        ++pattern.numRolls;
    }
}

} // namespace FillEngine
} // namespace rollforge
