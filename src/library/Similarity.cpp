#include "library/Similarity.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace rollforge
{
namespace Similarity
{

namespace
{
    /** A one-sided log that stays finite at zero and is monotonic. */
    float softLog (float x) noexcept { return std::log (1.0f + (x < 0.0f ? 0.0f : x)); }

    /** Power iteration for the dominant eigenvector of a symmetric matrix. Deterministic:
        the start vector is fixed, so the same covariance always yields the same axis. */
    void dominantEigenvector (const double cov[numFeatures][numFeatures], double out[numFeatures])
    {
        for (int i = 0; i < numFeatures; ++i)
            out[i] = 1.0 / std::sqrt ((double) numFeatures);   // fixed, not random

        for (int iter = 0; iter < 200; ++iter)
        {
            double next[numFeatures] {};
            for (int i = 0; i < numFeatures; ++i)
                for (int j = 0; j < numFeatures; ++j)
                    next[i] += cov[i][j] * out[j];

            double norm = 0.0;
            for (int i = 0; i < numFeatures; ++i)
                norm += next[i] * next[i];
            norm = std::sqrt (norm);

            if (norm < 1.0e-12)   // the matrix annihilates the vector: no variance left
                return;

            for (int i = 0; i < numFeatures; ++i)
                out[i] = next[i] / norm;
        }
    }

    /** Removes an axis from the covariance so the next power iteration finds the second one. */
    void deflate (double cov[numFeatures][numFeatures], const double axis[numFeatures])
    {
        // lambda = axis^T * cov * axis
        double lambda = 0.0;
        for (int i = 0; i < numFeatures; ++i)
            for (int j = 0; j < numFeatures; ++j)
                lambda += axis[i] * cov[i][j] * axis[j];

        for (int i = 0; i < numFeatures; ++i)
            for (int j = 0; j < numFeatures; ++j)
                cov[i][j] -= lambda * axis[i] * axis[j];
    }

    /** Pins an axis's direction: its largest-magnitude loading is made positive. Without
        this an eigenvector's sign is arbitrary and the map can come back mirrored. */
    void pinSign (double axis[numFeatures])
    {
        int   biggest = 0;
        double best   = 0.0;
        for (int i = 0; i < numFeatures; ++i)
            if (std::abs (axis[i]) > best)
            {
                best = std::abs (axis[i]);
                biggest = i;
            }

        if (axis[biggest] < 0.0)
            for (int i = 0; i < numFeatures; ++i)
                axis[i] = -axis[i];
    }
}

//==============================================================================
Vector featuresOf (const LibraryEntry& e) noexcept
{
    Vector v;
    v.v[logDuration] = softLog (e.durationSeconds);
    v.v[loudness]    = e.rms;
    v.v[brightness]  = e.zcr;
    v.v[sustain]     = e.decay;
    v.v[density]     = softLog ((float) e.onsetCount);
    return v;
}

Space build (const std::vector<LibraryEntry>& entries)
{
    Space space;
    if (entries.empty())
        return space;

    std::vector<Vector> raw;
    raw.reserve (entries.size());
    for (const auto& e : entries)
        raw.push_back (featuresOf (e));

    const float n = (float) raw.size();

    for (int f = 0; f < numFeatures; ++f)
    {
        double sum = 0.0;
        for (const auto& r : raw)
            sum += r.v[f];
        space.mean.v[f] = (float) (sum / (double) n);

        double sumSq = 0.0;
        for (const auto& r : raw)
        {
            const double d = (double) r.v[f] - (double) space.mean.v[f];
            sumSq += d * d;
        }
        const float sd = (float) std::sqrt (sumSq / (double) n);

        // A feature that never varies carries no information. Storing 1 here (rather than
        // dividing by ~0) keeps its normalised value at exactly 0 instead of exploding it
        // into the dominant axis, which is what a single-sample corpus would otherwise do.
        space.stdDev.v[f] = sd > 1.0e-6f ? sd : 1.0f;
    }

    space.normalised.reserve (raw.size());
    for (const auto& r : raw)
        space.normalised.push_back (normalise (space, r));

    return space;
}

Vector normalise (const Space& space, const Vector& raw) noexcept
{
    Vector out;
    for (int f = 0; f < numFeatures; ++f)
        out.v[f] = (raw.v[f] - space.mean.v[f]) / space.stdDev.v[f];
    return out;
}

float distanceSquared (const Vector& a, const Vector& b) noexcept
{
    float d = 0.0f;
    for (int f = 0; f < numFeatures; ++f)
    {
        const float delta = a.v[f] - b.v[f];
        d += delta * delta;
    }
    return d;
}

//==============================================================================
std::vector<int> nearestTo (const Space& space, const Vector& query, int k)
{
    std::vector<int> result;
    if (space.empty() || k <= 0)
        return result;

    std::vector<int> order ((std::size_t) space.size());
    std::iota (order.begin(), order.end(), 0);

    // Ties break by ascending index (stable_sort over an ascending order), so the same
    // corpus always answers the same way — a shuffling neighbour list feels broken.
    std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
    {
        return distanceSquared (space.normalised[(std::size_t) a], query)
             < distanceSquared (space.normalised[(std::size_t) b], query);
    });

    const int wanted = std::min (k, (int) order.size());
    result.assign (order.begin(), order.begin() + wanted);
    return result;
}

std::vector<int> nearest (const Space& space, int index, int k)
{
    std::vector<int> result;
    if (space.empty() || index < 0 || index >= space.size() || k <= 0)
        return result;

    // Ask for one extra, then drop the query itself — it is always its own nearest point.
    auto candidates = nearestTo (space, space.normalised[(std::size_t) index], k + 1);
    for (int i : candidates)
    {
        if (i == index)
            continue;
        result.push_back (i);
        if ((int) result.size() == k)
            break;
    }
    return result;
}

//==============================================================================
Feature Projection::dominantFeature (bool vertical) const noexcept
{
    const Vector& axis = vertical ? axis2 : axis1;
    int   best = 0;
    float mag  = 0.0f;
    for (int f = 0; f < numFeatures; ++f)
        if (std::abs (axis.v[f]) > mag)
        {
            mag  = std::abs (axis.v[f]);
            best = f;
        }
    return (Feature) best;
}

std::vector<Point2D> project2D (const Space& space)
{
    return project (space).points;
}

Projection project (const Space& space)
{
    Projection result;
    if (space.empty())
        return result;

    auto& points = result.points;
    points.resize ((std::size_t) space.size());

    // A corpus of one has no variance and no axes; put it in the middle rather than
    // dividing by a zero range.
    if (space.size() == 1)
    {
        points[0] = { 0.5f, 0.5f };
        return result;
    }

    // Covariance of the normalised data (which is already zero-mean).
    double cov[numFeatures][numFeatures] {};
    for (int i = 0; i < numFeatures; ++i)
        for (int j = 0; j < numFeatures; ++j)
        {
            double sum = 0.0;
            for (const auto& p : space.normalised)
                sum += (double) p.v[i] * (double) p.v[j];
            cov[i][j] = sum / (double) space.size();
        }

    double axis1[numFeatures] {};
    dominantEigenvector (cov, axis1);
    pinSign (axis1);

    deflate (cov, axis1);

    double axis2[numFeatures] {};
    dominantEigenvector (cov, axis2);
    pinSign (axis2);

    for (int f = 0; f < numFeatures; ++f)
    {
        result.axis1.v[f] = (float) axis1[f];
        result.axis2.v[f] = (float) axis2[f];
    }

    for (std::size_t i = 0; i < space.normalised.size(); ++i)
    {
        double x = 0.0, y = 0.0;
        for (int f = 0; f < numFeatures; ++f)
        {
            x += (double) space.normalised[i].v[f] * axis1[f];
            y += (double) space.normalised[i].v[f] * axis2[f];
        }
        points[i] = { (float) x, (float) y };
    }

    // Scale each axis into [0, 1] independently, so the map always fills its view.
    auto rescale = [&] (float Point2D::*member)
    {
        float lo =  1.0e30f, hi = -1.0e30f;
        for (const auto& p : points)
        {
            lo = std::min (lo, p.*member);
            hi = std::max (hi, p.*member);
        }

        const float range = hi - lo;
        for (auto& p : points)
            p.*member = range > 1.0e-6f ? (p.*member - lo) / range : 0.5f;
    };
    rescale (&Point2D::x);
    rescale (&Point2D::y);

    return result;
}

} // namespace Similarity
} // namespace rollforge
