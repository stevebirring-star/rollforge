// RollForge — LibraryDb round-trip tests (Phase 5, commit 2).
//
// Headless: an in-memory SQLite DB stores + returns entries, upsert replaces by
// path, category + favourites filters work.

#include "library/LibraryDb.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class LibraryDbTest final : public juce::UnitTest
{
public:
    LibraryDbTest() : juce::UnitTest ("RollForge LibraryDb", testCategory) {}

    void runTest() override
    {
        beginTest ("in-memory DB round-trips entries + filters by category");
        {
            LibraryDb db;
            expect (db.openInMemory());

            LibraryEntry kick;
            kick.path = "/x/kick.wav"; kick.name = "kick"; kick.category = SoundCategory::Kick;
            kick.rms = 0.5f; kick.durationSeconds = 0.3f;

            LibraryEntry hat;
            hat.path = "/x/hat.wav"; hat.name = "hat"; hat.category = SoundCategory::HatClosed;
            hat.zcr = 0.6f;

            expect (db.upsert (kick));
            expect (db.upsert (hat));
            expectEquals (db.count(), 2);

            const auto kicks = db.byCategory (SoundCategory::Kick);
            expectEquals ((int) kicks.size(), 1);
            expect (kicks[0].path == juce::String ("/x/kick.wav"));
            expectWithinAbsoluteError (kicks[0].rms, 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (kicks[0].durationSeconds, 0.3f, 1.0e-6f);

            expectEquals ((int) db.all().size(), 2);
        }

        beginTest ("upsert replaces by path; favourites filter");
        {
            LibraryDb db;
            expect (db.openInMemory());

            LibraryEntry snare;
            snare.path = "/x/snare.wav"; snare.name = "snare v1"; snare.category = SoundCategory::Snare;
            expect (db.upsert (snare));

            snare.name = "snare v2";
            expect (db.upsert (snare));          // same path -> replace, not a 2nd row
            expectEquals (db.count(), 1);
            expect (db.all()[0].name == juce::String ("snare v2"));

            expect (db.favourites().empty());
            expect (db.setFavourite ("/x/snare.wav", true));
            expectEquals ((int) db.favourites().size(), 1);
            expect (db.favourites()[0].favourite);
        }

        beginTest ("manual re-tag override survives re-scan + moves the sample");
        {
            LibraryDb db;
            expect (db.openInMemory());

            LibraryEntry s;
            s.path = "/x/mystery.wav"; s.name = "mystery"; s.category = SoundCategory::Perc;
            expect (db.upsert (s));
            expectEquals ((int) db.byCategory (SoundCategory::Perc).size(), 1);
            expect (db.byCategory (SoundCategory::Kick).empty());

            // Re-tag it as a kick: it moves category, and every query sees the override.
            expect (db.setCategoryOverride ("/x/mystery.wav", SoundCategory::Kick));
            expect (db.byCategory (SoundCategory::Perc).empty());
            expectEquals ((int) db.byCategory (SoundCategory::Kick).size(), 1);
            expect (db.all()[0].category == SoundCategory::Kick);

            // A re-scan (upsert with the ORIGINAL auto category) must NOT clobber it.
            s.category = SoundCategory::Perc;
            expect (db.upsert (s));
            expectEquals ((int) db.byCategory (SoundCategory::Kick).size(), 1);
            expect (db.byCategory (SoundCategory::Perc).empty());

            // Clearing the override restores the auto category.
            expect (db.clearCategoryOverride ("/x/mystery.wav"));
            expectEquals ((int) db.byCategory (SoundCategory::Perc).size(), 1);
            expect (db.byCategory (SoundCategory::Kick).empty());
        }
    }
};

static LibraryDbTest libraryDbTest;

} // namespace rollforge::tests
