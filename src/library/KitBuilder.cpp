#include "library/KitBuilder.h"

namespace rollforge
{

namespace
{
    // Fixed 4x4 drum layout (row-major). Pads 8..15 repeat useful categories so a
    // full kit is varied but coherent.
    const SoundCategory kPadLayout[kitNumPads] = {
        SoundCategory::Kick,  SoundCategory::Snare,     SoundCategory::HatClosed, SoundCategory::HatOpen,
        SoundCategory::Clap,  SoundCategory::Tom,       SoundCategory::Perc,      SoundCategory::Fx,
        SoundCategory::Kick,  SoundCategory::Snare,     SoundCategory::HatClosed, SoundCategory::Perc,
        SoundCategory::Tom,   SoundCategory::Clap,      SoundCategory::Perc,      SoundCategory::Fx
    };

    std::uint64_t splitmix (std::uint64_t& state) noexcept
    {
        state += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
}

KitBuilder::KitBuilder (const LibraryDb& dbToUse) : db (dbToUse) {}

SoundCategory KitBuilder::categoryForPad (int pad) noexcept
{
    if (pad < 0 || pad >= kitNumPads)
        return SoundCategory::Unknown;
    return kPadLayout[pad];
}

int KitBuilder::chokeGroupForPad (int pad) noexcept
{
    constexpr int hatChokeGroup = 1;   // all hat pads share this one group
    const SoundCategory c = categoryForPad (pad);
    return (c == SoundCategory::HatClosed || c == SoundCategory::HatOpen)
               ? hatChokeGroup : noChokeGroup;
}

KitBuilder::Selection KitBuilder::build (const Selection& current,
                                         std::uint64_t seed,
                                         const std::array<bool, kitNumPads>& locked) const
{
    Selection result;
    std::uint64_t state = seed;

    // Two pads of the same category (the two kicks, the three toms) draw from the same list.
    // Drawing independently means a 16-pad kit routinely lands the same sample twice, which
    // reads as a bug rather than as chance. Anything already spoken for -- including a locked
    // pad's sample -- is skipped, and only when a category has nothing left over do we allow
    // a repeat.
    juce::StringArray taken;

    for (int pad = 0; pad < kitNumPads; ++pad)
    {
        if (locked[(size_t) pad])
        {
            result.paths[(size_t) pad] = current.paths[(size_t) pad];   // keep
            if (result.paths[(size_t) pad].isNotEmpty())
                taken.add (result.paths[(size_t) pad]);
            continue;
        }

        const auto entries = db.byCategory (kPadLayout[pad]);
        if (entries.empty())
        {
            result.paths[(size_t) pad] = {};
            continue;
        }

        // Draw, then walk forward to the first sample nobody else has. Walking (rather than
        // re-drawing) keeps the whole build deterministic in the seed: exactly one random
        // number is consumed per unlocked pad, whatever the collisions.
        const std::uint64_t r     = splitmix (state);
        const int           count = (int) entries.size();
        const int           start = (int) (r % (std::uint64_t) count);

        int chosen = start;
        for (int step = 0; step < count; ++step)
        {
            const int candidate = (start + step) % count;
            if (! taken.contains (entries[(size_t) candidate].path))
            {
                chosen = candidate;
                break;
            }
        }

        result.paths[(size_t) pad] = entries[(size_t) chosen].path;
        taken.add (result.paths[(size_t) pad]);
    }

    return result;
}

} // namespace rollforge
