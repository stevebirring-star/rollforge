#pragma once

// RollForge — Pad: the per-pad model (pure parameters + a sample reference).
//
// MODEL LAYER RULE: no JUCE GUI includes. A Pad is plain, deterministic and
// unit-testable. It depends on engine/SampleBuffer only for the shared,
// GUI-free sample type (a reference-counted audio block) — never on any GUI.
//
// A Pad is edited on the MESSAGE THREAD and is part of a Kit (saved/loaded with
// a project). The audio thread never reads a Pad directly: the DrumEngine keeps
// its own real-time-safe copy of the parameters it needs, updated via the
// command FIFO (a later Phase-1 commit). Because a Pad holds SampleBuffer::Ptrs
// (reference-counted, non-trivial to copy), it is deliberately NOT a
// trivially-copyable POD — that constraint applies only to the audio-thread
// command payloads, not to this message-thread model.

#include "engine/SampleBuffer.h"

#include <array>

namespace rollforge
{

/** Choke group 0 means "no choke". Groups 1..N: triggering any pad in a group
    stops the others in the same group (e.g. a closed hat chokes the open hat). */
inline constexpr int noChokeGroup = 0;

/** Number of alternate samples a pad can round-robin / randomise between. */
inline constexpr int maxSampleAlternates = 4;

/** Playback pitch offset range, in semitones. */
inline constexpr float minPitchSemitones = -12.0f;
inline constexpr float maxPitchSemitones =  12.0f;

struct Pad
{
    // --- Sample -------------------------------------------------------------
    // Up to maxSampleAlternates alternates; alternates[0] is the primary sample.
    // A null entry is an unused slot. The alt picker / Humaniser (later phases)
    // choose which alternate a given hit uses; Phase 1 always plays alternates[0].
    std::array<SampleBuffer::Ptr, maxSampleAlternates> alternates {};

    // --- Per-pad parameters (neutral defaults) ------------------------------
    float volume     = 1.0f;              // linear gain (1 = unity)
    float pan        = 0.0f;              // -1 = hard left, 0 = centre, +1 = hard right
    float pitchSemis = 0.0f;              // pitch offset in semitones, [min..max]PitchSemitones
    float attackMs   = 0.0f;              // amplitude attack in ms (>= 0)
    float releaseMs  = 0.0f;              // amplitude release in ms (>= 0)
    int   chokeGroup = noChokeGroup;      // 0 = none, else a group id
    bool  reverse    = false;             // play the sample backwards
    float startFraction = 0.0f;           // trim: play from this fraction of the sample [0..1)
    float endFraction   = 1.0f;           // trim: ...to this fraction

    // --- Queries ------------------------------------------------------------
    /** The sample this pad plays by default (alternates[0]); null if empty. */
    const SampleBuffer::Ptr& primarySample() const noexcept { return alternates[0]; }

    /** True if the pad has at least one sample assigned. */
    bool hasSample() const noexcept
    {
        for (auto& s : alternates)
            if (s != nullptr)
                return true;
        return false;
    }

    /** How many alternate slots are currently filled. */
    int numAlternates() const noexcept
    {
        int n = 0;
        for (auto& s : alternates)
            if (s != nullptr)
                ++n;
        return n;
    }
};

} // namespace rollforge
