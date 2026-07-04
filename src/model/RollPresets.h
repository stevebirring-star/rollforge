#pragma once

// RollForge — RollPresets: 10 named roll shapes (Speed/Volume/Pitch curve
// presets) applied to a span/pad. Pure (no JUCE), deterministic.

#include "model/RollRegion.h"

namespace rollforge
{
namespace RollPresets
{
    enum Preset
    {
        TrapTriplet = 0,
        Buildup,
        Stutter,
        DrillSlide,
        MachineGun,
        FadeRoll,
        Crescendo,
        Decelerate,
        PitchRise,
        Ramp32,
        NumPresets
    };

    /** Human-readable name for a preset. */
    const char* name (Preset preset) noexcept;

    /** A RollRegion carrying the preset's Speed/Volume/Pitch curves, placed at
        `startStep` for `lengthSteps` on `targetPad`. */
    RollRegion make (Preset preset, double startStep, double lengthSteps, int targetPad);
}
} // namespace rollforge
