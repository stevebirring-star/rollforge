#pragma once

// RollForge — KitBuilder: "NEW KIT". Picks one sample per pad from the library DB
// following a fixed drum layout (kick / snare / hats / clap / tom / perc / fx),
// seeded so a given seed reproduces the same kit. Per-pad LOCKS keep chosen pads
// while the rest reroll. Pure selection (returns paths) — the caller loads them —
// so it is fully headless-testable. juce_core only.

#include "library/Categoriser.h"
#include "library/LibraryDb.h"
#include "model/Kit.h"

#include <array>
#include <cstdint>

namespace rollforge
{

class KitBuilder
{
public:
    explicit KitBuilder (const LibraryDb& db);

    /** Chosen sample path per pad ("" = nothing available for that pad). */
    struct Selection { std::array<juce::String, kitNumPads> paths {}; };

    /** Builds a selection: locked pads keep `current`; the rest pick a random
        sample from their pad's category. Deterministic for a given seed. */
    Selection build (const Selection& current,
                     std::uint64_t seed,
                     const std::array<bool, kitNumPads>& locked) const;

    /** The category assigned to each pad in the fixed layout. */
    static SoundCategory categoryForPad (int pad) noexcept;

    /** The choke group a freshly-built kit gives `pad`: every closed + open hat
        pad shares one group, so a closed hat cuts an open one automatically (no
        setup — unlike Atlas). All other pads report noChokeGroup. */
    static int chokeGroupForPad (int pad) noexcept;

private:
    const LibraryDb& db;
};

} // namespace rollforge
