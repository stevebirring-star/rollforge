#include "library/Categoriser.h"

namespace rollforge
{

const char* categoryName (SoundCategory category) noexcept
{
    switch (category)
    {
        case SoundCategory::Kick:      return "Kick";
        case SoundCategory::Snare:     return "Snare";
        case SoundCategory::Clap:      return "Clap";
        case SoundCategory::HatClosed: return "Hat (closed)";
        case SoundCategory::HatOpen:   return "Hat (open)";
        case SoundCategory::Tom:       return "Tom";
        case SoundCategory::Perc:      return "Perc";
        case SoundCategory::Fx:        return "FX";
        default:                       return "Unknown";
    }
}

namespace Categoriser
{

namespace
{
    // True if `s` (already lowercased) contains any of the given tokens.
    bool anyOf (const juce::String& s, std::initializer_list<const char*> tokens)
    {
        for (auto* t : tokens)
            if (s.contains (t))
                return true;
        return false;
    }
}

SoundCategory fromFilename (const juce::String& filename)
{
    const juce::String s = filename.toLowerCase();

    // Order matters: most specific tokens first (open hat before hat, etc.).
    if (anyOf (s, { "openhat", "open hat", "open-hat", "ohat", "hatopen" }))              return SoundCategory::HatOpen;
    if (anyOf (s, { "closedhat", "closed hat", "hihat", "hi-hat", "hi hat", "chh",
                    "closehat", "hat", "hh" }))                                            return SoundCategory::HatClosed;
    if (anyOf (s, { "kick", "kck", "bassdrum", "bass drum", "bd" }))                       return SoundCategory::Kick;
    if (anyOf (s, { "snare", "snr", "rimshot", "sd", "rim", "brush" }))                    return SoundCategory::Snare;
    if (anyOf (s, { "clap", "clp", "handclap" }))                                          return SoundCategory::Clap;
    if (anyOf (s, { "tom" }))                                                              return SoundCategory::Tom;
    if (anyOf (s, { "perc", "conga", "bongo", "shaker", "tamb", "cowbell", "clave",
                    "ride", "crash", "cymbal", "block", "triangle" }))                     return SoundCategory::Perc;
    if (anyOf (s, { "fx", "riser", "sweep", "noise", "impact", "reverse", "vinyl",
                    "foley", "downlifter", "uplifter" }))                                  return SoundCategory::Fx;

    return SoundCategory::Unknown;
}

SoundCategory fromFeatures (const AudioFeatures& f)
{
    // Bright (high zero-crossing rate) -> hats/cymbals; short vs long decay splits
    // closed vs open.
    if (f.zcr > 0.25f)
        return f.decay > 0.35f ? SoundCategory::HatOpen : SoundCategory::HatClosed;

    // Dark -> kick vs tom (kick has more sustained low-end body).
    if (f.zcr < 0.08f)
        return f.decay > 0.5f ? SoundCategory::Kick : SoundCategory::Tom;

    // Mid brightness: multiple transients -> clap; otherwise snare/perc.
    if (f.onsetCount >= 3)
        return SoundCategory::Clap;
    return f.decay > 0.4f ? SoundCategory::Snare : SoundCategory::Perc;
}

SoundCategory categorise (const juce::String& filename, const AudioFeatures& features)
{
    const SoundCategory byName = fromFilename (filename);
    if (byName != SoundCategory::Unknown)
        return byName;
    return fromFeatures (features);
}

} // namespace Categoriser
} // namespace rollforge
