#pragma once

// RollForge — Humaniser: one-knob Robot<->Human feel. Deterministic, seeded
// per-event jitter applied at PLAYBACK (non-destructive: it never edits the
// pattern). The Sequencer calls these when generating step events.
//
// Timing jitter is FORWARD-only (0 -> a little late), matching the event
// scheduler's forward-only constraint (backward "rush" needs the deferred
// look-ahead pass). Velocity jitter is bidirectional. Alt-sample jitter is
// deferred (needs Voice alternate-selection). Pure (no JUCE).

#include <cstdint>

namespace rollforge
{
namespace Humaniser
{
    /** A stable hash for an event. `salt` separates independent streams (e.g. 0
        for timing, 1 for velocity), so a step's timing and velocity jitter differ. */
    std::uint64_t eventHash (std::int64_t stepIndex, int lane, int salt) noexcept;

    /** Forward timing offset in STEPS for an event, 0..~0.2 step at amount 1.
        amount 0 -> 0. */
    float timingSteps (std::uint64_t eventHash, float amount) noexcept;

    /** Additive velocity jitter, +/- ~0.25 at amount 1. amount 0 -> 0. */
    float velocityDelta (std::uint64_t eventHash, float amount) noexcept;
}
} // namespace rollforge
