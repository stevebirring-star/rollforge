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

KitBuilder::Selection KitBuilder::build (const Selection& current,
                                         std::uint64_t seed,
                                         const std::array<bool, kitNumPads>& locked) const
{
    Selection result;
    std::uint64_t state = seed;

    for (int pad = 0; pad < kitNumPads; ++pad)
    {
        if (locked[(size_t) pad])
        {
            result.paths[(size_t) pad] = current.paths[(size_t) pad];   // keep
            continue;
        }

        const auto entries = db.byCategory (kPadLayout[pad]);
        if (entries.empty())
        {
            result.paths[(size_t) pad] = {};
            continue;
        }

        const std::uint64_t r = splitmix (state);
        const int idx = (int) (r % (std::uint64_t) entries.size());
        result.paths[(size_t) pad] = entries[(size_t) idx].path;
    }

    return result;
}

} // namespace rollforge
