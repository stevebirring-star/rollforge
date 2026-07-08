// RollForge — KitBuilder tests (Phase 5, commit 4).
//
// Headless: NEW KIT picks one sample per pad from its category, is deterministic
// per seed, respects per-pad locks, and yields empty pads when a category is empty.

#include "library/KitBuilder.h"
#include "library/LibraryDb.h"

#include <juce_core/juce_core.h>

#include <array>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    void add (LibraryDb& db, const juce::String& path, SoundCategory cat)
    {
        LibraryEntry e;
        e.path = path;
        e.name = path;
        e.category = cat;
        db.upsert (e);
    }
}

class KitBuilderTest final : public juce::UnitTest
{
public:
    KitBuilderTest() : juce::UnitTest ("RollForge KitBuilder", testCategory) {}

    void runTest() override
    {
        LibraryDb db;
        db.openInMemory();
        add (db, "/kick/a.wav",  SoundCategory::Kick);      add (db, "/kick/b.wav",  SoundCategory::Kick);
        add (db, "/snare/a.wav", SoundCategory::Snare);     add (db, "/snare/b.wav", SoundCategory::Snare);
        add (db, "/hc/a.wav",    SoundCategory::HatClosed); add (db, "/hc/b.wav",    SoundCategory::HatClosed);
        add (db, "/ho/a.wav",    SoundCategory::HatOpen);   add (db, "/ho/b.wav",    SoundCategory::HatOpen);
        add (db, "/clap/a.wav",  SoundCategory::Clap);      add (db, "/clap/b.wav",  SoundCategory::Clap);
        add (db, "/tom/a.wav",   SoundCategory::Tom);       add (db, "/tom/b.wav",   SoundCategory::Tom);
        add (db, "/perc/a.wav",  SoundCategory::Perc);      add (db, "/perc/b.wav",  SoundCategory::Perc);
        add (db, "/fx/a.wav",    SoundCategory::Fx);        add (db, "/fx/b.wav",    SoundCategory::Fx);

        KitBuilder kb (db);
        const KitBuilder::Selection empty;
        std::array<bool, kitNumPads> unlocked {};   // all false

        beginTest ("each pad gets a sample from its category; deterministic");
        {
            const auto sel = kb.build (empty, 42, unlocked);
            for (int p = 0; p < kitNumPads; ++p)
                expect (sel.paths[(size_t) p].isNotEmpty());
            expect (sel.paths[0] == juce::String ("/kick/a.wav")  || sel.paths[0] == juce::String ("/kick/b.wav"));
            expect (sel.paths[1] == juce::String ("/snare/a.wav") || sel.paths[1] == juce::String ("/snare/b.wav"));

            const auto sel2 = kb.build (empty, 42, unlocked);
            expect (sel.paths == sel2.paths);   // deterministic
        }

        beginTest ("locked pads keep their sample");
        {
            KitBuilder::Selection cur;
            cur.paths[0] = "/locked/kick.wav";
            std::array<bool, kitNumPads> lock {};
            lock[0] = true;

            const auto sel = kb.build (cur, 7, lock);
            expect (sel.paths[0] == juce::String ("/locked/kick.wav"));
            expect (sel.paths[1].isNotEmpty());   // other pads still filled
        }

        beginTest ("empty category -> empty pad; pad layout");
        {
            LibraryDb noEntries;
            noEntries.openInMemory();
            KitBuilder kb2 (noEntries);
            const auto sel = kb2.build (KitBuilder::Selection {}, 1, unlocked);
            for (int p = 0; p < kitNumPads; ++p)
                expect (sel.paths[(size_t) p].isEmpty());

            expect (KitBuilder::categoryForPad (0) == SoundCategory::Kick);
            expect (KitBuilder::categoryForPad (2) == SoundCategory::HatClosed);
        }

        beginTest ("all hat pads share one choke group; non-hats are unchoked");
        {
            const int hc  = KitBuilder::chokeGroupForPad (2);    // closed hat
            const int ho  = KitBuilder::chokeGroupForPad (3);    // open hat
            const int hc2 = KitBuilder::chokeGroupForPad (10);   // 2nd closed hat
            expect (hc != noChokeGroup);
            expectEquals (ho, hc);     // open hat shares the closed hat's group -> closed cuts open
            expectEquals (hc2, hc);    // the 2nd closed hat is in the same group
            expectEquals (KitBuilder::chokeGroupForPad (0), noChokeGroup);   // kick unchoked
            expectEquals (KitBuilder::chokeGroupForPad (1), noChokeGroup);   // snare unchoked
        }
    }
};

static KitBuilderTest kitBuilderTest;

} // namespace rollforge::tests
