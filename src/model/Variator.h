#pragma once

// RollForge — Variator: tasteful, seeded mutation of an EXISTING pattern.
//
// Where FillEngine generates a whole groove from scratch, the Variator evolves
// the current one: it nudges a few hits on/off, sprinkles ghost notes and varies
// accents — the verse -> chorus -> fill move that Atlas/XO can't do (they bounce
// separate loops). It is deterministic (same pattern + amount + seed -> identical
// result) so it is fully headless-tested, and it reports which cells changed so
// the UI can highlight them ("here's what I did"). Pure (no JUCE).
//
// Musical guardrails: anchor hits survive (kick on the downbeat, snare backbeats)
// so the groove never collapses, and locked lanes are left completely untouched.

#include "model/Pattern.h"

#include <array>
#include <cstdint>
#include <vector>

namespace rollforge
{
namespace Variator
{
    struct Change { int lane; int step; };   // a cell whose on/off state flipped

    /** Mutate `pattern` in place. `amount` (0..1) scales how much changes; `seed`
        makes it reproducible; a lane whose `lockedLanes` entry is true is skipped
        entirely. Returns the toggled cells, in lane/step order, for UI highlight. */
    std::vector<Change> vary (Pattern& pattern,
                              float amount,
                              std::uint64_t seed,
                              const std::array<bool, (std::size_t) maxLanes>& lockedLanes);
}
} // namespace rollforge
