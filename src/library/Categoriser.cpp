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
    /** Lowercases `raw` and reduces it to space-delimited words, so tokens can be matched
        on WORD boundaries rather than as substrings -- "Phat Kick" must not be a hat.

        Word breaks are inserted at punctuation, at letter<->digit boundaries ("kick01",
        "808kick") and, when `splitCamel`, at lowercase->uppercase humps ("KickDrum").
        The file extension is dropped so a ".fx" file is not FX. The result is padded with
        spaces at both ends, so `" " + token + " "` matches a whole word or phrase. */
    juce::String toWords (const juce::String& raw, bool splitCamel)
    {
        const juce::String base = raw.upToLastOccurrenceOf (".", false, false);

        juce::String spaced;
        bool prevAlnum = false, prevLower = false, prevDigit = false;

        for (auto c : base)
        {
            if (! juce::CharacterFunctions::isLetterOrDigit (c))
            {
                spaced += ' ';
                prevAlnum = prevLower = prevDigit = false;
                continue;
            }

            const bool isDigit = juce::CharacterFunctions::isDigit (c) != 0;
            const bool isUpper = juce::CharacterFunctions::isUpperCase (c) != 0;

            if (prevAlnum && ((splitCamel && isUpper && prevLower) || isDigit != prevDigit))
                spaced += ' ';

            spaced += juce::CharacterFunctions::toLowerCase (c);
            prevAlnum = true;
            prevDigit = isDigit;
            prevLower = ! isUpper && ! isDigit;
        }

        juce::StringArray words;
        words.addTokens (spaced, " ", "");
        words.removeEmptyStrings();
        return " " + words.joinIntoString (" ") + " ";
    }

    /** True if any token appears as a whole word (or whole phrase) in either spelling.
        Two forms are needed because both "OpenHat" and "openhat" are real filenames: the
        camel-split form matches the phrase token "open hat", the glued form matches
        "openhat". A token never matches a fragment of a longer word. */
    bool anyOf (const juce::String& spaced, const juce::String& glued,
                std::initializer_list<const char*> tokens)
    {
        for (auto* t : tokens)
        {
            const juce::String padded = " " + juce::String (t) + " ";
            if (spaced.contains (padded) || glued.contains (padded))
                return true;
        }
        return false;
    }
}

SoundCategory fromFilename (const juce::String& filename)
{
    const juce::String spaced = toWords (filename, true);    // "OpenHat_01" -> " open hat 01 "
    const juce::String glued  = toWords (filename, false);   // "OpenHat_01" -> " openhat 01 "

    // Order still matters: the open hat must be claimed before the plain "hat" token,
    // because the camel-split form of "OpenHat" contains " hat " too.
    if (anyOf (spaced, glued, { "openhat", "open hat", "ohat", "hatopen", "hat open" }))   return SoundCategory::HatOpen;
    if (anyOf (spaced, glued, { "closedhat", "closed hat", "closehat", "close hat",
                                "hihat", "hi hat", "chh", "hat", "hh" }))                  return SoundCategory::HatClosed;
    if (anyOf (spaced, glued, { "kick", "kck", "bassdrum", "bass drum", "bd" }))           return SoundCategory::Kick;
    if (anyOf (spaced, glued, { "snare", "snr", "rimshot", "rim shot", "sd", "rim",
                                "brush" }))                                                return SoundCategory::Snare;
    if (anyOf (spaced, glued, { "clap", "clp", "handclap", "hand clap" }))                 return SoundCategory::Clap;
    if (anyOf (spaced, glued, { "tom" }))                                                  return SoundCategory::Tom;
    if (anyOf (spaced, glued, { "perc", "conga", "bongo", "shaker", "tamb", "cowbell",
                                "clave", "ride", "crash", "cymbal", "block", "triangle" })) return SoundCategory::Perc;
    if (anyOf (spaced, glued, { "fx", "riser", "sweep", "noise", "impact", "reverse",
                                "vinyl", "foley", "downlifter", "uplifter" }))             return SoundCategory::Fx;

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
