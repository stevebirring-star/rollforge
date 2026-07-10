// RollForge — Similarity tests (P2: find-similar + the constellation browser).
//
// Headless + pure. Two properties matter more than any distance number:
//
//   * DETERMINISM. The same corpus must give the same neighbours and the same map, every
//     time, including the SIGN of each principal axis. A map that mirrors between runs
//     destroys the user's memory of where their sounds live.
//   * SCALE INVARIANCE OF THE INPUTS. Duration is in seconds, ZCR is a 0..1 ratio. Without
//     z-scoring, "similar" would mean "similar length" and nothing else.

#include "library/Similarity.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    LibraryEntry entry (const char* name, float duration, float rms, float zcr,
                        float decay, int onsets)
    {
        LibraryEntry e;
        e.path            = juce::String ("/x/") + name;
        e.name            = name;
        e.durationSeconds = duration;
        e.rms             = rms;
        e.zcr             = zcr;
        e.decay           = decay;
        e.onsetCount      = onsets;
        return e;
    }

    /** A small corpus with two obvious clusters: heavy dark kicks, and short bright hats. */
    std::vector<LibraryEntry> twoClusterCorpus()
    {
        return {
            entry ("kick_a", 0.40f, 0.32f, 0.02f, 0.30f, 1),
            entry ("kick_b", 0.44f, 0.30f, 0.03f, 0.33f, 1),
            entry ("kick_c", 0.38f, 0.34f, 0.02f, 0.28f, 1),
            entry ("hat_a",  0.05f, 0.10f, 0.42f, 0.06f, 1),
            entry ("hat_b",  0.06f, 0.11f, 0.45f, 0.07f, 1),
            entry ("hat_c",  0.04f, 0.09f, 0.40f, 0.05f, 1),
        };
    }
}

class SimilarityTest final : public juce::UnitTest
{
public:
    SimilarityTest() : juce::UnitTest ("RollForge Similarity", testCategory) {}

    void runTest() override
    {
        beginTest ("an empty or single-entry corpus is safe");
        {
            const auto empty = Similarity::build ({});
            expect (empty.empty());
            expect (Similarity::nearest (empty, 0, 3).empty());
            expect (Similarity::project2D (empty).empty());

            const auto one = Similarity::build ({ entry ("solo", 0.3f, 0.2f, 0.1f, 0.2f, 1) });
            expectEquals (one.size(), 1);
            expect (Similarity::nearest (one, 0, 3).empty(), "nothing is near a lone sound");

            const auto map = Similarity::project2D (one);
            expectEquals ((int) map.size(), 1);
            expectWithinAbsoluteError (map[0].x, 0.5f, 1.0e-6f, "a lone sound sits in the middle");
            expectWithinAbsoluteError (map[0].y, 0.5f, 1.0e-6f);
        }

        beginTest ("a feature that never varies cannot dominate the distance");
        {
            // Every entry has the same duration. Its standard deviation is zero, and a naive
            // divide would send that axis to infinity (or NaN) and swamp everything else.
            std::vector<LibraryEntry> corpus;
            for (int i = 0; i < 5; ++i)
                corpus.push_back (entry ("s", 0.25f, 0.1f * (float) i, 0.05f * (float) i, 0.2f, 1));

            const auto space = Similarity::build (corpus);
            for (const auto& p : space.normalised)
            {
                expect (! std::isnan (p.v[Similarity::logDuration]));
                expectWithinAbsoluteError (p.v[Similarity::logDuration], 0.0f, 1.0e-5f,
                                           "a constant feature normalises to exactly zero");
            }
        }

        beginTest ("z-scoring makes every feature count, whatever unit it came in");
        {
            const auto space = Similarity::build (twoClusterCorpus());
            expectEquals (space.size(), 6);

            for (int f = 0; f < Similarity::numFeatures; ++f)
            {
                double mean = 0.0, var = 0.0;
                for (const auto& p : space.normalised)
                    mean += p.v[f];
                mean /= (double) space.size();

                for (const auto& p : space.normalised)
                    var += ((double) p.v[f] - mean) * ((double) p.v[f] - mean);
                var /= (double) space.size();

                expectWithinAbsoluteError ((float) mean, 0.0f, 1.0e-4f);
                // Zero-variance features (onsets, all 1 here) stay at 0; the rest reach 1.
                if (f != Similarity::density)
                    expectWithinAbsoluteError ((float) std::sqrt (var), 1.0f, 1.0e-4f);
            }
        }

        beginTest ("a kick's neighbours are kicks, and a hat's are hats");
        {
            const auto corpus = twoClusterCorpus();
            const auto space  = Similarity::build (corpus);

            for (int i = 0; i < 3; ++i)          // the three kicks
                for (int n : Similarity::nearest (space, i, 2))
                    expect (n < 3, corpus[(std::size_t) i].name + "'s neighbour "
                                       + corpus[(std::size_t) n].name + " is not a kick");

            for (int i = 3; i < 6; ++i)          // the three hats
                for (int n : Similarity::nearest (space, i, 2))
                    expect (n >= 3, corpus[(std::size_t) i].name + "'s neighbour "
                                        + corpus[(std::size_t) n].name + " is not a hat");
        }

        beginTest ("nearest() never returns the query itself, and is stable under ties");
        {
            // Four identical entries: every distance is zero, so only the tie-break decides.
            std::vector<LibraryEntry> same (4, entry ("same", 0.2f, 0.3f, 0.1f, 0.2f, 1));
            const auto space = Similarity::build (same);

            const auto a = Similarity::nearest (space, 2, 3);
            const auto b = Similarity::nearest (space, 2, 3);
            expectEquals ((int) a.size(), 3);
            expect (std::find (a.begin(), a.end(), 2) == a.end(), "the query is never its own neighbour");
            expect (a == b, "an identical corpus must answer identically twice");
            expect (a == (std::vector<int> { 0, 1, 3 }), "ties break by ascending index");
        }

        beginTest ("k larger than the corpus returns everything else, not garbage");
        {
            const auto space = Similarity::build (twoClusterCorpus());
            const auto all = Similarity::nearest (space, 0, 99);
            expectEquals ((int) all.size(), 5);
            expect (std::find (all.begin(), all.end(), 0) == all.end());
        }

        beginTest ("nearestTo() finds a home for a sound that isn't in the corpus");
        {
            const auto corpus = twoClusterCorpus();
            const auto space  = Similarity::build (corpus);

            // A sound shaped like a hat, never scanned.
            const auto query = Similarity::normalise (space,
                Similarity::featuresOf (entry ("stranger", 0.05f, 0.10f, 0.43f, 0.06f, 1)));

            const auto near = Similarity::nearestTo (space, query, 3);
            expectEquals ((int) near.size(), 3);
            for (int n : near)
                expect (n >= 3, "the stranger landed among the kicks");
        }

        beginTest ("the 2D map separates the clusters and fills its view");
        {
            const auto space  = Similarity::build (twoClusterCorpus());
            const auto points = Similarity::project2D (space);
            expectEquals ((int) points.size(), 6);

            float lo = 1.0f, hi = 0.0f;
            for (const auto& p : points)
            {
                expect (p.x >= -1.0e-5f && p.x <= 1.0f + 1.0e-5f, "x is normalised into [0,1]");
                expect (p.y >= -1.0e-5f && p.y <= 1.0f + 1.0e-5f, "y is normalised into [0,1]");
                lo = std::min (lo, p.x);
                hi = std::max (hi, p.x);
            }
            expectWithinAbsoluteError (lo, 0.0f, 1.0e-5f, "the map touches both edges");
            expectWithinAbsoluteError (hi, 1.0f, 1.0e-5f);

            // The two clusters must land apart along the dominant axis: every kick on one
            // side of the midpoint, every hat on the other. That IS the map being useful.
            const bool kicksLow = points[0].x < 0.5f;
            for (int i = 0; i < 3; ++i)
                expect ((points[(std::size_t) i].x < 0.5f) == kicksLow, "the kicks scattered");
            for (int i = 3; i < 6; ++i)
                expect ((points[(std::size_t) i].x < 0.5f) != kicksLow, "a hat sits among the kicks");
        }

        beginTest ("the map is deterministic, sign and all");
        {
            // A PCA axis's sign is mathematically arbitrary. Left unpinned, the same library
            // could render mirrored on a different run and the user's mental map would flip.
            const auto corpus = twoClusterCorpus();
            const auto first  = Similarity::project2D (Similarity::build (corpus));
            const auto second = Similarity::project2D (Similarity::build (corpus));

            expectEquals ((int) first.size(), (int) second.size());
            for (std::size_t i = 0; i < first.size(); ++i)
            {
                expectWithinAbsoluteError (first[i].x, second[i].x, 1.0e-6f);
                expectWithinAbsoluteError (first[i].y, second[i].y, 1.0e-6f);
            }
        }

        beginTest ("each axis's sign is pinned, so the map cannot come back mirrored");
        {
            // Running project() twice would agree even with the sign left free, because the
            // power iteration starts from a fixed vector. The property that actually holds
            // the map still is this one: an axis's largest loading is always positive.
            const auto proj = Similarity::project (Similarity::build (twoClusterCorpus()));

            for (bool vertical : { false, true })
            {
                const auto& axis = vertical ? proj.axis2 : proj.axis1;
                const auto  dom  = proj.dominantFeature (vertical);
                expect (axis.v[dom] > 0.0f,
                        juce::String (vertical ? "vertical" : "horizontal")
                            + " axis has a negative dominant loading");
            }
        }

        beginTest ("the two axes are orthogonal (deflation worked)");
        {
            // If deflation failed, the second power iteration would return the first axis
            // again and the whole map would collapse onto its own diagonal.
            const auto proj = Similarity::project (Similarity::build (twoClusterCorpus()));

            float dot = 0.0f, len1 = 0.0f, len2 = 0.0f;
            for (int f = 0; f < Similarity::numFeatures; ++f)
            {
                dot  += proj.axis1.v[f] * proj.axis2.v[f];
                len1 += proj.axis1.v[f] * proj.axis1.v[f];
                len2 += proj.axis2.v[f] * proj.axis2.v[f];
            }
            expectWithinAbsoluteError (dot, 0.0f, 1.0e-3f, "the axes are not perpendicular");
            expectWithinAbsoluteError (std::sqrt (len1), 1.0f, 1.0e-3f, "axis 1 is not a unit vector");
            expectWithinAbsoluteError (std::sqrt (len2), 1.0f, 1.0e-3f, "axis 2 is not a unit vector");

            // And the map really does use both: points must spread on each axis.
            float maxDiff = 0.0f;
            for (const auto& p : proj.points)
                maxDiff = std::max (maxDiff, std::abs (p.x - p.y));
            expect (maxDiff > 0.05f, "the map collapsed onto its diagonal");
        }
    }
};

static SimilarityTest similarityTest;

} // namespace rollforge::tests
