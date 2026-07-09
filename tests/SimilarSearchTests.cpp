// RollForge — SimilarSearch tests (P2: the per-pad "Similar" button).
//
// Headless + pure. The behaviours that matter are the ones a musician would notice:
//
//   * A kick's neighbours are kicks. Never a tom, however close in feature space.
//   * No similar sound is better than a wrong one — an empty answer, not a category hop.
//   * The list is stable, so pressing "Similar" repeatedly walks it instead of shuffling.
//   * A sound the library has never seen can still find its cousins.

#include "library/SimilarSearch.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    LibraryEntry entry (const char* name, SoundCategory cat, float duration, float rms,
                        float zcr, float decay, int onsets = 1)
    {
        LibraryEntry e;
        e.path            = juce::String ("/samples/") + name + ".wav";
        e.name            = name;
        e.durationSeconds = duration;
        e.rms             = rms;
        e.zcr             = zcr;
        e.decay           = decay;
        e.onsetCount      = onsets;
        e.category        = cat;
        return e;
    }

    /** Four kicks and two hats. kick_near is deliberately closest to kick_query, and a hat
        sits closer to nothing — the point is that category, not distance, gates the answer. */
    std::vector<LibraryEntry> corpus()
    {
        return {
            entry ("kick_query", SoundCategory::Kick,      0.40f, 0.30f, 0.02f, 0.30f),
            entry ("kick_near",  SoundCategory::Kick,      0.41f, 0.30f, 0.02f, 0.31f),
            entry ("kick_mid",   SoundCategory::Kick,      0.50f, 0.26f, 0.05f, 0.40f),
            entry ("kick_far",   SoundCategory::Kick,      0.90f, 0.14f, 0.12f, 0.75f),
            entry ("hat_a",      SoundCategory::HatClosed, 0.05f, 0.10f, 0.42f, 0.06f),
            entry ("hat_b",      SoundCategory::HatClosed, 0.06f, 0.11f, 0.45f, 0.07f),
        };
    }

    juce::String leaf (const juce::String& path)
    {
        return juce::File (path).getFileNameWithoutExtension();
    }
}

class SimilarSearchTest final : public juce::UnitTest
{
public:
    SimilarSearchTest() : juce::UnitTest ("RollForge SimilarSearch", testCategory) {}

    void runTest() override
    {
        beginTest ("an empty library answers nothing, and does not crash doing it");
        {
            SimilarSearch s;
            expect (s.isEmpty());
            expectEquals (s.size(), 0);
            expectEquals (s.indexOf ("/samples/anything.wav"), -1);
            expect (s.neighboursOf ("/samples/anything.wav", 5).isEmpty());

            AudioFeatures f;
            f.durationSeconds = 0.4f;
            expect (s.neighboursOf ("kick.wav", f, 5).isEmpty());
        }

        beginTest ("a path the library has never seen has no neighbours by path");
        {
            SimilarSearch s;
            s.rebuild (corpus());
            expect (s.neighboursOf ("/samples/ghost.wav", 3).isEmpty(),
                    "an unknown path must not silently match something");
        }

        beginTest ("neighbours are ordered by distance, nearest first");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            const auto n = s.neighboursOf ("/samples/kick_query.wav", 3);
            expectEquals (n.size(), 3);
            expectEquals (leaf (n[0]), juce::String ("kick_near"));
            expectEquals (leaf (n[1]), juce::String ("kick_mid"));
            expectEquals (leaf (n[2]), juce::String ("kick_far"));
        }

        beginTest ("the query is never its own neighbour");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            const auto n = s.neighboursOf ("/samples/kick_query.wav", 99);
            expectEquals (n.size(), 3, "only the other three kicks");
            expect (! n.contains ("/samples/kick_query.wav"));
        }

        beginTest ("a kick's neighbours are only ever kicks");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            // Ask for far more than the category holds. A distance-only search would top up
            // the list with hats; this must not.
            for (const auto& p : s.neighboursOf ("/samples/kick_far.wav", 99))
                expect (leaf (p).startsWith ("kick"), leaf (p) + " is not a kick");
        }

        beginTest ("a lone sound of its kind has no neighbours, and we say so");
        {
            std::vector<LibraryEntry> c = corpus();
            c.push_back (entry ("clap_alone", SoundCategory::Clap, 0.12f, 0.2f, 0.3f, 0.15f, 2));

            SimilarSearch s;
            s.rebuild (c);
            expect (s.neighboursOf ("/samples/clap_alone.wav", 5).isEmpty(),
                    "the only clap must return nothing, not the nearest kick");
        }

        beginTest ("repeated queries walk the same list, so Similar cycles instead of shuffling");
        {
            // Three kicks at exactly the same point: only the tie-break orders them.
            std::vector<LibraryEntry> c {
                entry ("k0", SoundCategory::Kick, 0.4f, 0.3f, 0.02f, 0.3f),
                entry ("k1", SoundCategory::Kick, 0.4f, 0.3f, 0.02f, 0.3f),
                entry ("k2", SoundCategory::Kick, 0.4f, 0.3f, 0.02f, 0.3f),
                entry ("k3", SoundCategory::Kick, 0.4f, 0.3f, 0.02f, 0.3f),
            };

            SimilarSearch s;
            s.rebuild (c);
            const auto a = s.neighboursOf ("/samples/k2.wav", 3);
            const auto b = s.neighboursOf ("/samples/k2.wav", 3);
            expect (a == b, "two identical queries disagreed");
            expectEquals (leaf (a[0]), juce::String ("k0"));
            expectEquals (leaf (a[1]), juce::String ("k1"));
            expectEquals (leaf (a[2]), juce::String ("k3"));
        }

        beginTest ("a sound the library has never seen still finds its cousins");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            // Shaped like a kick, and named like one. It is in neither the DB nor the space.
            AudioFeatures f;
            f.durationSeconds = 0.42f;
            f.rms             = 0.29f;
            f.zcr             = 0.02f;
            f.decay           = 0.31f;
            f.onsetCount      = 1;

            const auto n = s.neighboursOf ("BD_909.wav", f, 2);
            expectEquals (n.size(), 2);
            for (const auto& p : n)
                expect (leaf (p).startsWith ("kick"), leaf (p) + " is not a kick");

            // ...and all four kicks are candidates now, because none of them IS the query.
            expectEquals (s.neighboursOf ("BD_909.wav", f, 99).size(), 4);
        }

        beginTest ("an entry query excludes its own path, even when the library holds it");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            // This is the pad's route in: Scanner::analyseFile hands back a full row for a
            // file that may or may not already be in the library. Either way it must not be
            // offered back as a suggestion for itself.
            LibraryEntry q = entry ("kick_query", SoundCategory::Kick, 0.40f, 0.30f, 0.02f, 0.30f);
            const auto n = s.neighboursOf (q, 99);
            expectEquals (n.size(), 3);
            expect (! n.contains (q.path), "the pad's own sound came back as its own suggestion");
        }

        beginTest ("an outside sound is categorised the way the Scanner would categorise it");
        {
            SimilarSearch s;
            s.rebuild (corpus());

            // Hat-shaped, hat-named: it must land among the hats, not the kicks.
            AudioFeatures f;
            f.durationSeconds = 0.05f;
            f.rms             = 0.10f;
            f.zcr             = 0.43f;
            f.decay           = 0.06f;
            f.onsetCount      = 1;

            const auto n = s.neighboursOf ("closed_hat_01.wav", f, 5);
            expectEquals (n.size(), 2, "both hats, and neither kick");
            for (const auto& p : n)
                expect (leaf (p).startsWith ("hat"), leaf (p) + " is not a hat");
        }

        beginTest ("the space is normalised over the WHOLE library, not per category");
        {
            // Duration is in seconds and ZCR is a 0..1 ratio. If the space were rebuilt per
            // category, the two kicks below would be ordered by whichever feature happened to
            // vary within kicks. Globally z-scored, brightness carries real weight: kick_dark
            // differs from the query by 0.01 in duration, kick_bright by 0.10 in zcr, and the
            // library's zcr spread (0.02..0.45) is far narrower than its duration spread.
            std::vector<LibraryEntry> c {
                entry ("q",          SoundCategory::Kick,      0.40f, 0.30f, 0.02f, 0.30f),
                entry ("kick_dark",  SoundCategory::Kick,      0.41f, 0.30f, 0.02f, 0.30f),
                entry ("kick_bright",SoundCategory::Kick,      0.40f, 0.30f, 0.12f, 0.30f),
                entry ("hat_a",      SoundCategory::HatClosed, 0.05f, 0.10f, 0.42f, 0.06f),
                entry ("hat_b",      SoundCategory::HatClosed, 2.00f, 0.11f, 0.45f, 0.07f),
            };

            SimilarSearch s;
            s.rebuild (c);
            const auto n = s.neighboursOf ("/samples/q.wav", 2);
            expectEquals (leaf (n[0]), juce::String ("kick_dark"),
                          "the darker kick should win: brightness is expensive in this library");
        }

        beginTest ("rebuild() replaces the corpus, and clear() empties it");
        {
            SimilarSearch s;
            s.rebuild (corpus());
            expectEquals (s.size(), 6);

            s.rebuild (std::vector<LibraryEntry> { entry ("only", SoundCategory::Fx, 1.0f, 0.1f, 0.2f, 0.5f) });
            expectEquals (s.size(), 1);
            expectEquals (s.indexOf ("/samples/kick_query.wav"), -1, "the old corpus lingered");

            s.clear();
            expect (s.isEmpty());
        }
    }
};

static SimilarSearchTest similarSearchTest;

} // namespace rollforge::tests
