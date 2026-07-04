#pragma once

// RollForge — StarterKit: 16 drum sounds synthesised in code (no binary assets).
//
// Produces a fully-populated Kit whose 16 pads each hold a procedurally generated
// SampleBuffer, plus sensible defaults and choke groups (the two hats share one,
// so a closed hat chokes the open hat). Synthesis is deterministic (fixed RNG
// seeds), so the kit is identical every run. Build it once on a non-audio thread;
// the Voice resamples per-voice, so it does not need rebuilding on a device-rate
// change.
//
// LIBRARY LAYER: no JUCE GUI includes.

#include "model/Kit.h"

namespace rollforge
{
namespace StarterKit
{
    // Pad layout on the 4x4 grid (row-major, row 0 = top).
    enum PadIndex
    {
        Kick = 0,  Snare = 1,   ClosedHat = 2, OpenHat = 3,
        Clap = 4,  Rim = 5,     LowTom = 6,    MidTom = 7,
        HighTom = 8, Crash = 9, Ride = 10,     Cowbell = 11,
        Shaker = 12, Perc1 = 13, Perc2 = 14,   Kick2 = 15,
    };

    // The closed + open hats share this choke group.
    inline constexpr int hatChokeGroup = 1;

    /** Builds a 16-pad Kit with all sounds synthesised at `sampleRate`. */
    Kit build (double sampleRate);
}
} // namespace rollforge
