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

        beginTest ("no sample lands on two pads when the category has enough to go round");
        {
            // Perc is the tightest layout: pads 6, 11 and 14 all want one. Give it four.
            LibraryDb rich;
            rich.openInMemory();
            for (const auto* c : { "a", "b", "c", "d" })
            {
                add (rich, juce::String ("/kick/")  + c + ".wav", SoundCategory::Kick);
                add (rich, juce::String ("/snare/") + c + ".wav", SoundCategory::Snare);
                add (rich, juce::String ("/hc/")    + c + ".wav", SoundCategory::HatClosed);
                add (rich, juce::String ("/ho/")    + c + ".wav", SoundCategory::HatOpen);
                add (rich, juce::String ("/clap/")  + c + ".wav", SoundCategory::Clap);
                add (rich, juce::String ("/tom/")   + c + ".wav", SoundCategory::Tom);
                add (rich, juce::String ("/perc/")  + c + ".wav", SoundCategory::Perc);
                add (rich, juce::String ("/fx/")    + c + ".wav", SoundCategory::Fx);
            }

            KitBuilder kbRich (rich);
            for (std::uint64_t seed = 1; seed <= 40; ++seed)
            {
                const auto sel = kbRich.build (empty, seed, unlocked);
                juce::StringArray seen;
                for (int p = 0; p < kitNumPads; ++p)
                {
                    expect (! seen.contains (sel.paths[(size_t) p]),
                            "seed " + juce::String (seed) + ": " + sel.paths[(size_t) p]
                                + " was dealt to two pads");
                    seen.add (sel.paths[(size_t) p]);
                }
            }
        }

        beginTest ("a category with fewer samples than pads repeats, rather than leaving a pad empty");
        {
            // Two kick pads (0 and 8), one kick. A pad with no sound is worse than a twin.
            LibraryDb thin;
            thin.openInMemory();
            add (thin, "/kick/only.wav", SoundCategory::Kick);

            KitBuilder kbThin (thin);
            const auto sel = kbThin.build (empty, 3, unlocked);
            expectEquals (sel.paths[0], juce::String ("/kick/only.wav"));
            expectEquals (sel.paths[8], juce::String ("/kick/only.wav"));
        }

        beginTest ("an unlocked pad will not steal a locked pad's sample");
        {
            // Pad 0 is locked to /kick/a.wav, so pad 8 (the other kick) must take /kick/b.wav.
            KitBuilder::Selection cur;
            cur.paths[0] = "/kick/a.wav";
            std::array<bool, kitNumPads> lock {};
            lock[0] = true;

            for (std::uint64_t seed = 1; seed <= 20; ++seed)
            {
                const auto sel = kb.build (cur, seed, lock);
                expectEquals (sel.paths[0], juce::String ("/kick/a.wav"));
                expectEquals (sel.paths[8], juce::String ("/kick/b.wav"),
                              "seed " + juce::String (seed) + ": pad 8 duplicated the locked kick");
            }
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
