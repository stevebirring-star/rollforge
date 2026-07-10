#include "library/SimilarSearch.h"

#include <algorithm>
#include <numeric>

namespace rollforge
{

void SimilarSearch::rebuild (const LibraryDb& db)
{
    rebuild (db.isOpen() ? db.all() : std::vector<LibraryEntry> {});
}

void SimilarSearch::rebuild (std::vector<LibraryEntry> entriesIn)
{
    corpus     = std::move (entriesIn);
    normalised = Similarity::build (corpus);
}

void SimilarSearch::clear()
{
    corpus.clear();
    normalised = {};
}

int SimilarSearch::indexOf (const juce::String& path) const
{
    for (std::size_t i = 0; i < corpus.size(); ++i)
        if (corpus[i].path == path)
            return (int) i;
    return -1;
}

juce::StringArray SimilarSearch::neighboursOf (const juce::String& path, int k) const
{
    const int index = indexOf (path);
    if (index < 0)
        return {};

    return rank (normalised.normalised[(std::size_t) index],
                 corpus[(std::size_t) index].category,
                 index, k);
}

juce::StringArray SimilarSearch::neighboursOf (const LibraryEntry& query, int k) const
{
    if (corpus.empty())
        return {};

    // Exclude the query's own row if the library happens to hold it: a pad's sound is
    // never its own suggestion, whichever route the caller came in by.
    return rank (Similarity::normalise (normalised, Similarity::featuresOf (query)),
                 query.category, indexOf (query.path), k);
}

juce::StringArray SimilarSearch::neighboursOf (const juce::String& name,
                                               const AudioFeatures& features,
                                               int k) const
{
    // Reconstruct the row the Scanner would have written, so an outside sound is judged
    // by exactly the same rules as one that was scanned.
    LibraryEntry query;
    query.name            = name;
    query.durationSeconds = features.durationSeconds;
    query.rms             = features.rms;
    query.zcr             = features.zcr;
    query.decay           = features.decay;
    query.onsetCount      = features.onsetCount;
    query.category        = Categoriser::categorise (name, features);

    return neighboursOf (query, k);
}

juce::StringArray SimilarSearch::rank (const Similarity::Vector& query,
                                       SoundCategory category,
                                       int excludeIndex,
                                       int k) const
{
    juce::StringArray result;
    if (corpus.empty() || k <= 0)
        return result;

    std::vector<int> candidates;
    candidates.reserve (corpus.size());
    for (int i = 0; i < (int) corpus.size(); ++i)
        if (i != excludeIndex && corpus[(std::size_t) i].category == category)
            candidates.push_back (i);

    // Nothing else of this kind. Say so by returning nothing, rather than reaching into a
    // neighbouring category and handing a tom to someone who asked about a kick.
    if (candidates.empty())
        return result;

    // stable_sort over an already-ascending list gives the index tie-break for free, so
    // two equally-close samples always come back in the same order.
    std::stable_sort (candidates.begin(), candidates.end(), [&] (int a, int b)
    {
        return Similarity::distanceSquared (normalised.normalised[(std::size_t) a], query)
             < Similarity::distanceSquared (normalised.normalised[(std::size_t) b], query);
    });

    const int wanted = std::min (k, (int) candidates.size());
    for (int i = 0; i < wanted; ++i)
        result.add (corpus[(std::size_t) candidates[(std::size_t) i]].path);

    return result;
}

} // namespace rollforge
