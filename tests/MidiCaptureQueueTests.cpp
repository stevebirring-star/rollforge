// RollForge — MidiCaptureQueue tests.
//
// Headless, and the last one is a real multi-producer race: two "MIDI threads" push while the
// message thread drains. Run under ThreadSanitizer (ROLLFORGE_TSAN); that is what makes the
// claim in the header ("safe from several MIDI threads at once") worth anything.

#include "engine/MidiCaptureQueue.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <thread>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class MidiCaptureQueueTest final : public juce::UnitTest
{
public:
    MidiCaptureQueueTest() : juce::UnitTest ("RollForge MidiCaptureQueue", testCategory) {}

    void runTest() override
    {
        beginTest ("notes come out in the order they went in, once");
        {
            MidiCaptureQueue q (8);
            q.push ({ 3, 0.5f, 100 });
            q.push ({ 5, 0.9f, 220 });

            expectEquals (q.size(), 2);

            const auto out = q.take();
            expectEquals ((int) out.notes.size(), 2);
            expectEquals (out.notes[0].pad, 3);
            expectEquals (out.notes[1].pad, 5);
            expectEquals (out.notes[1].transportSample, (std::int64_t) 220);
            expectWithinAbsoluteError (out.notes[0].velocity, 0.5f, 1.0e-6f);

            expectEquals (q.size(), 0, "take() must empty the queue");
            expect (q.take().notes.empty(), "the same notes came out twice");
        }

        beginTest ("take() leaves the queue able to accept notes without allocating");
        {
            // The bug this pins: swapping the buffer out on take() leaves capacity zero, and
            // the very next push -- on a MIDI thread -- allocates.
            MidiCaptureQueue q (64);
            const int before = q.capacity();
            expect (before >= 64);

            q.push ({ 1, 1.0f, 0 });
            q.take();

            expectEquals (q.capacity(), before, "take() threw the reserved buffer away");

            for (int i = 0; i < 64; ++i)
                q.push ({ i % 16, 1.0f, i });
            expectEquals (q.size(), 64);
            expectEquals (q.take().dropped, 0, "it dropped notes it had room for");
            expectEquals (q.capacity(), before, "it grew");
        }

        beginTest ("a full queue drops notes and counts them, rather than growing");
        {
            MidiCaptureQueue q (4);
            for (int i = 0; i < 4; ++i)
                q.push ({ i, 1.0f, i });

            q.push ({ 9, 1.0f, 99 });
            q.push ({ 9, 1.0f, 99 });
            expectEquals (q.size(), 4, "the queue grew past its capacity");

            const auto out = q.take();
            expectEquals ((int) out.notes.size(), 4);
            expectEquals (out.dropped, 2, "the drops were not counted");
            expectEquals (out.notes[3].pad, 3, "a dropped note displaced a kept one");
            expectEquals (q.take().dropped, 0, "take() must forget the drops");
        }

        beginTest ("clear() throws away the take, and the drops with it");
        {
            MidiCaptureQueue q (2);
            q.push ({ 1, 1.0f, 0 });
            q.push ({ 2, 1.0f, 0 });
            q.push ({ 3, 1.0f, 0 });   // dropped

            q.clear();
            expectEquals (q.size(), 0);
            const auto after = q.take();
            expect (after.notes.empty());
            expectEquals (after.dropped, 0, "clear() must forget the drops");

            q.push ({ 7, 1.0f, 0 });
            expectEquals (q.size(), 1, "clear() left the queue unusable");
        }

        beginTest ("two MIDI threads and one drain: nothing is lost, invented or torn");
        {
            constexpr int perThread = 4000;
            MidiCaptureQueue q (16);   // deliberately tiny: most pushes WILL be dropped

            std::atomic<bool> go { false };
            std::atomic<int>  pushed { 0 };

            auto producer = [&] (int padBase)
            {
                while (! go.load (std::memory_order_acquire)) {}
                for (int i = 0; i < perThread; ++i)
                {
                    q.push ({ padBase, 1.0f, i });
                    pushed.fetch_add (1, std::memory_order_acq_rel);
                }
            };

            std::thread a (producer, 1);
            std::thread b (producer, 2);

            int collected = 0, drops = 0;
            go.store (true, std::memory_order_release);

            auto drain = [&]
            {
                const auto batch = q.take();
                collected += (int) batch.notes.size();
                drops     += batch.dropped;
                for (const auto& n : batch.notes)
                    expect (n.pad == 1 || n.pad == 2, "a note came out of the queue torn in half");
            };

            while (pushed.load (std::memory_order_acquire) < 2 * perThread)
                drain();

            a.join();
            b.join();
            drain();   // whatever landed after the producers finished

            expect (collected > 0, "the drain never saw anything");
            expect (collected <= 2 * perThread, "the queue invented notes");

            // The invariant the Batch API exists for: every note pushed either came out or was
            // counted as dropped. Reading the two separately loses exactly the notes dropped
            // between the reads, which is precisely when notes are dropped.
            expectEquals (collected + drops, 2 * perThread,
                          "notes vanished without being counted as dropped");
        }
    }
};

static MidiCaptureQueueTest midiCaptureQueueTest;

} // namespace rollforge::tests
