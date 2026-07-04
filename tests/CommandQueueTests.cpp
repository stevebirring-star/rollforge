// RollForge — CommandQueue unit tests.
//
// Headless checks of the lock-free SPSC command queue: FIFO ordering, empty-drain
// no-op, full-queue behaviour (push fails without corrupting queued items), and
// correct wrap-around across many push/drain cycles. (Single-threaded here — the
// concurrency contract is enforced by usage; these prove the ring logic.)

#include "engine/CommandQueue.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class CommandQueueTest final : public juce::UnitTest
{
public:
    CommandQueueTest() : juce::UnitTest ("RollForge CommandQueue", testCategory) {}

    void runTest() override
    {
        beginTest ("push then drain preserves FIFO order");
        {
            CommandQueue q (16);
            for (int i = 0; i < 5; ++i)
                expect (q.push (EngineCommand::makeTrigger (i, (float) i / 10.0f)));

            expectEquals (q.getNumReady(), 5);

            juce::Array<int> seen;
            q.drain ([&] (const EngineCommand& c) { seen.add (c.padIndex); });

            expectEquals (q.getNumReady(), 0);
            expectEquals (seen.size(), 5);
            for (int i = 0; i < 5; ++i)
                expectEquals (seen[i], i);
        }

        beginTest ("drain on an empty queue is a no-op");
        {
            CommandQueue q (8);
            int count = 0;
            q.drain ([&] (const EngineCommand&) { ++count; });
            expectEquals (count, 0);
        }

        beginTest ("push returns false when full, without losing queued items");
        {
            CommandQueue q (4);
            int pushed = 0;
            while (q.push (EngineCommand::makeTrigger (pushed, 1.0f)))
                ++pushed;

            expect (pushed >= 1);
            expectEquals (q.getFreeSpace(), 0);
            expect (! q.push (EngineCommand::makeTrigger (99, 1.0f)));   // full -> rejected

            // Everything that reported success is still retrievable, in order.
            juce::Array<int> seen;
            q.drain ([&] (const EngineCommand& c) { seen.add (c.padIndex); });
            expectEquals (seen.size(), pushed);
            for (int i = 0; i < pushed; ++i)
                expectEquals (seen[i], i);
        }

        beginTest ("wraps correctly across many push/drain cycles");
        {
            CommandQueue q (4);
            int next = 0, expected = 0;

            for (int cycle = 0; cycle < 20; ++cycle)
            {
                for (int k = 0; k < 2; ++k)
                    expect (q.push (EngineCommand::makeTrigger (next++, 1.0f)));

                q.drain ([&] (const EngineCommand& c) { expectEquals (c.padIndex, expected++); });
            }

            expectEquals (q.getNumReady(), 0);
            expectEquals (next, expected);
        }
    }
};

static CommandQueueTest commandQueueTest;

} // namespace rollforge::tests
