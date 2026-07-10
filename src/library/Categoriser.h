#pragma once

// RollForge — Categoriser: maps a sample to a drum category using its filename
// tokens first (fast + reliable for named packs), then an audio-feature-rule
// fallback for un-obvious names. Pure model (juce_core only, no GUI) so it is
// headless-testable.
//
// Tokens match on WORD boundaries, not as substrings, or "Phat Kick" is a hat.
// Words are split at punctuation, at letter<->digit boundaries and at camelCase
// humps, so "kick01", "808kick" and "KickDrum" all still read as a kick. A name
// that boundary-matching leaves Unknown (an all-lowercase compound like
// "kickdrum") falls through to the feature rules, which is why categorise() --
// not fromFilename() -- is what callers should use.

#include "library/FeatureExtractor.h"

#include <juce_core/juce_core.h>

namespace rollforge
{

enum class SoundCategory
{
    Kick, Snare, Clap, HatClosed, HatOpen, Tom, Perc, Fx, Unknown
};

const char* categoryName (SoundCategory category) noexcept;

namespace Categoriser
{
    /** Filename-token match first; falls back to feature rules if no token hits. */
    SoundCategory categorise (const juce::String& filename, const AudioFeatures& features);

    /** Filename-only classification (Unknown if no token matches). */
    SoundCategory fromFilename (const juce::String& filename);

    /** Feature-only classification (used when the filename is unhelpful). */
    SoundCategory fromFeatures (const AudioFeatures& features);
}

} // namespace rollforge
