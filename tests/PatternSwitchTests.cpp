// RollForge — PatternBank + queued pattern-switch tests (Phase 2, commit 5).
//
// Headless: a queued pattern must swap in exactly on the bar boundary while
// playing (not before), and immediately while stopped. Uses an empty pattern A
// (fires nothing) and a pattern B (fires at bar starts) so the trigger count and
// the isSwitchQueued flag pin the switch timing.

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"
#include "model/PatternBank.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    Pattern firesAtBarStart()   // 1 lane, step 0 on, length 16 -> fires at steps 0,16,32...
    {
        Pattern p;
        p.numLanes = 1;
        p.lane (0).targetPad = 0;
        p.lane (0).length = 16;
        p.lane (0).step (0).on = true;
        return p;
    }
}

class PatternSwitchTest final : public juce::UnitTest
{
public:
    PatternSwitchTest() : juce::UnitTest ("RollForge PatternSwitch", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("PatternBank defaults and accessors");
        {
            PatternBank bank;
            expectEquals (numPatternSlots, 8);
            expectEquals ((int) bank.slots.size(), 8);
            expectEquals (bank.currentSlot, 0);
            expectEquals (bank.queuedSlot, -1);
            expect (PatternBank::isValidSlot (0));
            expect (PatternBank::isValidSlot (7));
            expect (! PatternBank::isValidSlot (-1));
            expect (! PatternBank::isValidSlot (8));

            bank.pattern (3).bpm = 140.0;
            expectWithinAbsoluteError (bank.slots[3].bpm, 140.0, 1.0e-9);
        }

        beginTest ("a queued switch lands on the bar boundary, not before");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            const Pattern empty;               // fires nothing
            const Pattern b = firesAtBarStart();

            seq.setPattern (empty);
            seq.setPlaying (true);

            int samplePos = 0;
            auto processTo = [&] (int target)
            {
                while (samplePos < target)
                {
                    const int n = juce::jmin (512, target - samplePos);
                    juce::AudioBuffer<float> buf (1, n);
                    buf.clear();
                    seq.process (engine, buf);
                    samplePos += n;
                }
            };

            processTo (1024);
            expectEquals (seq.getTriggerCount(), (std::int64_t) 0);

            seq.queuePattern (b);
            // Bar boundary (step 16) at round(16 * 5512.5) = 88200 samples.
            processTo (88000);
            expect (seq.isSwitchQueued());
            expectEquals (seq.getTriggerCount(), (std::int64_t) 0);   // still the empty pattern

            processTo (90000);
            expect (! seq.isSwitchQueued());                          // swapped on the bar
            expect (seq.getTriggerCount() >= 1);                      // B's bar-start hit fired
        }

        beginTest ("a queued switch applies immediately while stopped");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            seq.setPlaying (false);
            seq.queuePattern (firesAtBarStart());

            juce::AudioBuffer<float> buf (1, 512);
            buf.clear();
            seq.process (engine, buf);            // stopped -> applied at once
            expect (! seq.isSwitchQueued());

            // Now play across step 0: the (already active) pattern fires.
            seq.setPlaying (true);
            juce::AudioBuffer<float> buf2 (1, 1024);
            buf2.clear();
            seq.process (engine, buf2);
            expect (seq.getTriggerCount() >= 1);
        }

        beginTest ("getSwitchCount only counts queued patterns that actually became active");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPattern (Pattern {});
            seq.setPlaying (true);

            int samplePos = 0;
            auto processTo = [&] (int target)
            {
                while (samplePos < target)
                {
                    const int n = juce::jmin (512, target - samplePos);
                    juce::AudioBuffer<float> buf (1, n);
                    buf.clear();
                    seq.process (engine, buf);
                    samplePos += n;
                }
            };

            processTo (1024);
            const auto before = seq.getSwitchCount();

            // The UI reads the counter, THEN queues. Between those two lines and the next
            // audio block, isSwitchQueued() is still false — a caller watching that flag
            // would conclude the switch had already landed. The counter cannot lie that way.
            seq.queuePattern (firesAtBarStart());
            expect (! seq.isSwitchQueued(), "the audio thread has not seen the queue yet");
            expectEquals (seq.getSwitchCount(), before, "nothing has landed yet");

            processTo (88000);                 // mid-bar: queued, not yet swapped
            expect (seq.isSwitchQueued());
            expectEquals (seq.getSwitchCount(), before, "it landed before the bar line");

            processTo (90000);                 // past the bar line at 88200
            expectEquals (seq.getSwitchCount(), before + 1, "the switch did not register");

            // An immediate setPattern is not a queued switch and must not bump the counter,
            // or every step edit the user makes would look like a slot change.
            seq.setPattern (Pattern {});
            processTo (95000);
            expectEquals (seq.getSwitchCount(), before + 1, "setPattern counted as a switch");
        }

        beginTest ("a switch queued while stopped counts the moment it is applied");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setPattern (Pattern {});
            seq.setPlaying (false);

            const auto before = seq.getSwitchCount();
            seq.queuePattern (firesAtBarStart());

            juce::AudioBuffer<float> buf (1, 512);
            buf.clear();
            seq.process (engine, buf);

            expect (! seq.isSwitchQueued());
            expectEquals (seq.getSwitchCount(), before + 1,
                          "a stopped transport applies at once, and that still counts");
        }

        beginTest ("setPattern replaces immediately (no bar wait)");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            seq.setPattern (Pattern {});        // empty
            seq.setPlaying (true);
            seq.setPattern (firesAtBarStart()); // immediate replace, even mid-play

            juce::AudioBuffer<float> buf (1, 1024);
            buf.clear();
            seq.process (engine, buf);          // step 0 at sample 0 fires right away
            expect (! seq.isSwitchQueued());
            expect (seq.getTriggerCount() >= 1);
        }
    }
};

static PatternSwitchTest patternSwitchTest;

} // namespace rollforge::tests
