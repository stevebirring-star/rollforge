#pragma once

// RollForge — Similarity: "which of these sounds like that one?", over the features the
// library already stores.
//
// The Scanner has always written duration / rms / zcr / decay / onsets into the DB, and
// nothing has ever read them except the Categoriser's fallback rules. This turns them into
// a small metric space, which is all two of the roadmap's Later items actually need:
//
//   * nearest()   — the k closest sounds to a given one. Per-pad "find similar".
//   * project2D() — the corpus flattened onto a plane by PCA. The constellation browser.
//
// Both answer from the SAME normalised space, so a sample that sits next to another on the
// map really is its neighbour. Two different distance functions would be a lie told twice.
//
// HONEST LIMIT: these are coarse, hand-crafted, time-domain features. They separate a kick
// from a hat easily, and two similar kicks only roughly. XO and Atlas use learned
// embeddings and will win a head-to-head on nuance. The value here is ergonomic — one tap
// to swap a sound for a plausible cousin — not a claim to beat them at retrieval.
//
// Pure: no JUCE GUI, no allocation on any audio path (nothing here runs on one). All of it
// is deterministic: the same corpus always yields the same neighbours and the same map,
// including the SIGN of each principal axis, which a naive PCA leaves free to flip.
//
// LIBRARY LAYER: juce_core only.

#include "library/LibraryDb.h"

#include <cstdint>
#include <vector>

namespace rollforge
{

namespace Similarity
{
    /** The five things the library knows about a sound. Ordered; index is meaningful. */
    enum Feature
    {
        logDuration = 0,   // how long it rings
        loudness,          // rms
        brightness,        // zero-crossing rate
        sustain,           // fraction of the sample above 10% of peak — low = percussive
        density,           // log(1 + onsets) — one hit, or a roll?
        numFeatures
    };

    /** A word for a feature, for labelling the map's axes. */
    const char* featureName (Feature feature) noexcept;

    struct Vector
    {
        float v[numFeatures] {};
    };

    struct Point2D
    {
        float x = 0.0f, y = 0.0f;
    };

    /** Raw (un-normalised) features of one entry. */
    Vector featuresOf (const LibraryEntry& entry) noexcept;

    /** A corpus, z-scored so that no single feature dominates the distance purely because
        it happens to be measured in seconds rather than in a 0..1 ratio. */
    struct Space
    {
        std::vector<Vector> normalised;   // one per input entry, same order
        Vector mean {};
        Vector stdDev {};                 // a zero-variance feature is stored as 1 and zeroed

        bool empty() const noexcept { return normalised.empty(); }
        int  size()  const noexcept { return (int) normalised.size(); }
    };

    Space build (const std::vector<LibraryEntry>& entries);

    /** Squared distance between two points of the space. */
    float distanceSquared (const Vector& a, const Vector& b) noexcept;

    /** Indices of the `k` entries closest to `index`, nearest first. Never returns `index`
        itself. Ties break by ascending index, so the result is stable. */
    std::vector<int> nearest (const Space& space, int index, int k);

    /** Indices of the `k` entries closest to an arbitrary point (already normalised into
        the space — use normalise()). Used to find a neighbour for a sound that is on a pad
        but may not be in the corpus. */
    std::vector<int> nearestTo (const Space& space, const Vector& normalisedQuery, int k);

    /** Maps raw features into the space's normalised coordinates. */
    Vector normalise (const Space& space, const Vector& raw) noexcept;

    /** The corpus flattened onto a plane, plus the two axes it was flattened along. */
    struct Projection
    {
        std::vector<Point2D> points;   // each coordinate scaled into [0, 1]
        Vector axis1 {};               // loadings of the horizontal axis
        Vector axis2 {};               // ...and the vertical one, orthogonal to it

        /** The feature that contributes most to an axis — what to label it with. */
        Feature dominantFeature (bool vertical) const noexcept;
    };

    /** PCA the corpus onto a plane.

        The two axes are the directions of greatest variance, so nearby points really are
        similar sounds. Each axis's sign is pinned by forcing its largest-magnitude loading
        positive — a principal axis's sign is mathematically arbitrary, and left free the
        identical library could render mirrored, flipping the user's mental map of where
        their sounds live. */
    Projection project (const Space& space);

    /** Just the points. */
    std::vector<Point2D> project2D (const Space& space);
}

} // namespace rollforge
