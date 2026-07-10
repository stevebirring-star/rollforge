#pragma once

// RollForge — SimilarSearch: "give me another sound like this one", answered from the
// library the user already scanned.
//
// This is the ergonomic layer over Similarity. It holds the corpus and its normalised
// space, and turns a *path* into an ordered list of *paths*. Two rules make the answer
// something a musician would accept:
//
//   * SAME CATEGORY, ALWAYS. A "Similar" button on a kick that hands back a tom reads as
//     broken, however close the two sit in feature space. The category is a hard filter;
//     the features only order what survives it. If nothing else shares the category, the
//     answer is empty — better an honest "nothing similar" than a confident wrong sound.
//   * ONE GLOBAL SPACE. Z-scoring happens over the WHOLE library, then the category
//     filter is applied. Normalising per-category instead would make "similar" mean
//     something different inside each drum type.
//
// A sound that is not in the library (a file dragged onto a pad, a starter-kit sound) can
// still be queried: pass its analysed features and name, and its category is inferred
// exactly the way the Scanner would have inferred it.
//
// Message-thread use. Rebuild after a scan. Pure: juce_core only, no GUI, no audio thread.

#include "library/Categoriser.h"
#include "library/FeatureExtractor.h"
#include "library/LibraryDb.h"
#include "library/Similarity.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace rollforge
{

class SimilarSearch
{
public:
    SimilarSearch() = default;

    /** Reloads the corpus and re-normalises the space. Call after a scan or a re-tag. */
    void rebuild (const LibraryDb& db);

    /** Same, from an explicit corpus (tests, and anything that already has the rows). */
    void rebuild (std::vector<LibraryEntry> corpus);

    void clear();

    bool isEmpty() const noexcept { return corpus.empty(); }
    int  size()    const noexcept { return (int) corpus.size(); }

    const std::vector<LibraryEntry>& entries() const noexcept { return corpus; }
    const Similarity::Space&         space()   const noexcept { return normalised; }

    /** Paths of the `k` library samples most like the one at `path`, nearest first.

        `path` itself never appears in the result. Restricted to `path`'s category as
        recorded in the library. Empty if `path` is not in the library — use the
        features overload for a sound the library has never seen. */
    juce::StringArray neighboursOf (const juce::String& path, int k) const;

    /** Paths of the `k` library samples most like a sound the library does not hold —
        a file dropped straight onto a pad, say. Feed it a row from Scanner::analyseFile
        and the answer is exactly the one a scanned copy would have got. `query.path`
        never appears in the result, even if the library does happen to hold it. */
    juce::StringArray neighboursOf (const LibraryEntry& query, int k) const;

    /** Same, when all you have is the filename and its features: the category is inferred
        the way the Scanner would infer it. */
    juce::StringArray neighboursOf (const juce::String& name,
                                    const AudioFeatures& features,
                                    int k) const;

    /** Index of `path` in entries(), or -1. */
    int indexOf (const juce::String& path) const;

private:
    /** Ranks every entry of `category` (bar `excludeIndex`) by distance from `query`.
        Ties break by ascending corpus index, so repeated presses walk a stable list. */
    juce::StringArray rank (const Similarity::Vector& query,
                            SoundCategory category,
                            int excludeIndex,
                            int k) const;

    std::vector<LibraryEntry> corpus;
    Similarity::Space         normalised;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimilarSearch)
};

} // namespace rollforge
