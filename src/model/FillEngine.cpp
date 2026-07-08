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
    };

    // Pad indices (match the StarterKit layout).
    constexpr int Kick = 0, Snare = 1, CHat = 2, OHat = 3, Clap = 4,
                  LowTom = 6, MidTom = 7, HighTom = 8, Ride = 10, Shaker = 12;

    constexpr std::uint16_t bit (int step) noexcept { return (std::uint16_t) (1u << step); }
    bool  has  (std::uint16_t mask, int step) noexcept { return ((mask >> step) & 1u) != 0; }
    float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    // A curated per-style recipe. The genre *skeleton* (anchors) is always placed;
    // *options* are candidate steps that fire with a probability scaled by intensity.
    // Because the skeleton alone is already genre-correct, every dice roll lands on a
    // usable beat — the point of "curated" randomisation (vs Playbeat's noise).
    struct Profile
    {
        std::uint16_t kickAnchors;                 // always-on kick steps
        std::uint16_t kickOptions;  float kickProb;// syncopated kick candidates
        int           backPad;                     // backbeat instrument (Snare or Clap)
        std::uint16_t backAnchors;                 // always-on backbeat steps (the "2 & 4")
        std::uint16_t ghostSteps;   float ghostProb; // quiet snare ghosts
        int           hatBase;                     // closed-hat spacing at intensity 3 (steps)
        std::uint16_t hatAccents;                  // hats that hit harder
        std::uint16_t hatRolls;     int  rollRatchets; // hats that RATCHET (the rolling-hat signature)
        std::uint16_t openHats;                    // open-hat steps (house/techno offbeats)
        int           percPad;                     // -1 = none
        std::uint16_t percSteps;    float percProb;
        float         swing;                        // per-genre feel (boom-bap swings hardest)
    };

    Profile profileFor (Style style) noexcept
    {
        const std::uint16_t off8 = bit (2) | bit (6) | bit (10) | bit (14);   // offbeat 8ths
        const std::uint16_t beats = bit (0) | bit (4) | bit (8) | bit (12);   // the four beats
        const std::uint16_t four = beats;                                     // four-on-the-floor

        switch (style)
        {
            case Trap: return {
                bit (0), bit (3) | bit (6) | bit (10) | bit (11) | bit (14), 0.40f,
                Clap, bit (4) | bit (12),
                bit (7) | bit (15), 0.30f,
                1, beats, bit (7) | bit (15), 3,
                0,
                -1, 0, 0.0f,
                0.10f };

            case Drill: return {
                bit (0), bit (3) | bit (6) | bit (10) | bit (13), 0.45f,
                Snare, bit (4) | bit (11),                       // snare on 4 + the "and" = drill bounce
                bit (7) | bit (14), 0.30f,
                1, off8, bit (3) | bit (11), 3,
                0,
                -1, 0, 0.0f,
                0.08f };

            case House: return {
                four, 0, 0.0f,                                   // four-on-the-floor
                Clap, bit (4) | bit (12),
                0, 0.0f,
                4, 0, 0, 0,
                off8,                                            // offbeat open hats = house signature
                -1, 0, 0.0f,
                0.0f };

            case DnB: return {
                bit (0), bit (6) | bit (10), 0.50f,              // broken kick
                Snare, bit (4) | bit (12),
                bit (2) | bit (7) | bit (9) | bit (14) | bit (15), 0.35f,  // breakbeat ghosts
                1, off8, bit (15), 2,
                0,
                Ride, off8, 0.30f,
                0.0f };

            case BoomBap: return {
                bit (0), bit (6) | bit (7) | bit (10), 0.45f,
                Snare, bit (4) | bit (12),
                bit (3) | bit (7) | bit (14), 0.30f,
                2, beats, 0, 0,                                  // 8th-note dusty hats, no rolls
                0,
                -1, 0, 0.0f,
                0.55f };                                         // the swing that makes boom-bap

            case Techno: return {
                four, 0, 0.0f,
                Clap, bit (4) | bit (12),
                0, 0.0f,
                4, 0, 0, 0,
                off8,                                            // offbeat open hats
                Shaker, bit (1) | bit (3) | bit (5) | bit (7) | bit (9) | bit (11) | bit (13) | bit (15), 0.40f,
                0.0f };

            case Pop: return {
                bit (0) | bit (8), bit (10), 0.30f,              // kick on 1 & 3
                Snare, bit (4) | bit (12),
                0, 0.0f,
                2, beats, 0, 0,                                  // clean 8th hats
                0,
                -1, 0, 0.0f,
                0.0f };

            default: return profileFor (Trap);
        }
    }

    void setupLane (Pattern& p, int laneIndex, int targetPad)
    {
        p.lane (laneIndex).targetPad = targetPad;
        p.lane (laneIndex).length    = 16;
        for (int s = 0; s < maxStepsPerLane; ++s)
            p.lane (laneIndex).step (s) = Step {};   // clear
    }

    bool isOn (const Pattern& p, int lane, int step) noexcept
    {
        return step >= 0 && step < 16 && p.lane (lane).step (step).on;
    }

    void put (Pattern& p, int lane, int step, float velocity, int ratchets = 1)
    {
        if (step >= 0 && step < 16)
        {
            Step& st = p.lane (lane).step (step);
            st.on          = true;
            st.velocity    = velocity;
            st.ratchets    = ratchets;
            st.ratchetRamp = ratchets > 1 ? 0.3f : 0.0f;   // slight crescendo across a roll
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
    const Profile pf = profileFor (style);

    // Full 16-lane layout, lane i -> pad i, so a generated beat drops straight onto
    // the rows the grid already shows and every genre role lands on the right pad
    // (kick, snare/clap, closed/open hat, ride, shaker, toms).
    pattern.numLanes = maxLanes;
    pattern.numRolls = 0;
    pattern.swing    = pf.swing;
    for (int lane = 0; lane < maxLanes; ++lane)
        setupLane (pattern, lane, lane);

    // How strongly the optional (probabilistic) hits fire, by intensity.
    const float optScale = clampf ((float) intensity / 3.0f, 0.2f, 1.7f);

    // --- Kick: genre skeleton + intensity-gated syncopation --------------------
    for (int s = 0; s < 16; ++s)
        if (has (pf.kickAnchors, s))
            put (pattern, Kick, s, 0.95f);
    for (int s = 0; s < 16; ++s)
        if (has (pf.kickOptions, s) && ! isOn (pattern, Kick, s)
            && rng.chance (clampf (pf.kickProb * optScale, 0.0f, 0.95f)))
            put (pattern, Kick, s, 0.88f);

    // --- Backbeat (snare or clap) on the "2 & 4" -------------------------------
    for (int s = 0; s < 16; ++s)
        if (has (pf.backAnchors, s))
            put (pattern, pf.backPad, s, 0.9f);

    // --- Ghost snares: quiet, off the beat -------------------------------------
    for (int s = 0; s < 16; ++s)
        if (has (pf.ghostSteps, s) && ! isOn (pattern, Snare, s)
            && rng.chance (clampf (pf.ghostProb * optScale, 0.0f, 0.9f)))
            put (pattern, Snare, s, 0.3f);

    // --- Closed hats: spacing tightens with intensity; accents + rolling hats ---
    const int every = (int) clampf ((float) (pf.hatBase + (3 - intensity)), 1.0f, 8.0f);
    for (int s = 0; s < 16; s += every)
    {
        if (isOn (pattern, CHat, s))
            continue;
        const int   rat = has (pf.hatRolls, s) ? pf.rollRatchets : 1;
        const float vel = has (pf.hatAccents, s) ? 0.72f : 0.5f;
        put (pattern, CHat, s, vel, rat);
    }

    // --- Open hats (house/techno offbeats; choke against the closed hat) --------
    for (int s = 0; s < 16; ++s)
        if (has (pf.openHats, s))
            put (pattern, OHat, s, 0.6f);

    // --- Extra percussion (dnb ride, techno shaker) ----------------------------
    if (pf.percPad >= 0)
        for (int s = 0; s < 16; ++s)
            if (has (pf.percSteps, s)
                && rng.chance (clampf (pf.percProb * optScale, 0.0f, 0.9f)))
                put (pattern, pf.percPad, s, 0.45f);

    // --- Tom fill in the last 4 steps, denser at higher intensity --------------
    const int tomLanes[3] = { HighTom, MidTom, LowTom };
    for (int s = 12; s < 16; ++s)
        if (rng.chance (clampf (0.12f + 0.09f * (float) intensity, 0.0f, 0.9f)))
            put (pattern, tomLanes[(s - 12) % 3], s, 0.82f);

    // --- Signature roll at the end for intensity >= 3 --------------------------
    if (intensity >= 3 && pattern.numRolls < maxRolls)
    {
        const RollRegion r = RollPresets::make (rollForStyle (style), 14.0, 2.0, Snare);
        pattern.rolls[(size_t) pattern.numRolls] = RollCompiler::compile (r);
        ++pattern.numRolls;
    }
}

} // namespace FillEngine
} // namespace rollforge
