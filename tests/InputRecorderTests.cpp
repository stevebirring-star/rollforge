// RollForge — InputRecorder tests.
//
// Headless, and one of them is a real two-thread race hunt. The class exists to move samples
// off the audio thread safely; a test that only calls it from one thread proves nothing about
// the thing it is for. Run the suite under ThreadSanitizer (ROLLFORGE_TSAN) and this is the
// test that earns its keep.

#include "engine/InputRecorder.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cmath>
#include <thread>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    /** Feeds one block of a constant value on every channel. */
    void feed (InputRecorder& r, float value, int numChannels, int numSamples,
               std::int64_t transportSample = 0)
    {
        std::vector<std::vector<float>> data ((std::size_t) numChannels,
                                              std::vector<float> ((std::size_t) numSamples, value));
        std::vector<const float*> pointers ((std::size_t) numChannels);
        for (int c = 0; c < numChannels; ++c)
            pointers[(std::size_t) c] = data[(std::size_t) c].data();

        r.writeBlock (pointers.data(), numChannels, numSamples, transportSample);
    }
}

class InputRecorderTest final : public juce::UnitTest
{
public:
    InputRecorderTest() : juce::UnitTest ("RollForge InputRecorder", testCategory) {}

    void runTest() override
    {
        beginTest ("a disarmed recorder ignores everything the audio thread hands it");
        {
            InputRecorder r;
            r.prepare (44100.0, 1.0);

            feed (r, 1.0f, 2, 512);
            expectEquals (r.samplesCaptured(), 0, "it recorded while disarmed");
            expect (r.take().empty());
        }

        beginTest ("null and empty blocks are survived, not trusted");
        {
            InputRecorder r;
            r.prepare (44100.0, 1.0);
            r.start();

            r.writeBlock (nullptr, 2, 512, 0);
            feed (r, 1.0f, 0, 512);
            feed (r, 1.0f, 2, 0);
            expectEquals (r.samplesCaptured(), 0);

            // A channel pointer that is null among valid ones must not be dereferenced.
            std::vector<float> real ((std::size_t) 64, 0.5f);
            const float* pointers[2] = { real.data(), nullptr };
            r.writeBlock (pointers, 2, 64, 0);
            expectEquals (r.samplesCaptured(), 64);

            const auto out = r.take();
            expectEquals ((int) out.size(), 64);
            expectWithinAbsoluteError (out[0], 0.25f, 1.0e-6f, "the null channel was counted but not read");
        }

        beginTest ("the input is summed to mono");
        {
            InputRecorder r;
            r.prepare (44100.0, 1.0);
            r.start();
            feed (r, 0.8f, 2, 100);          // both channels 0.8 -> mono 0.8
            const auto out = r.take();

            expectEquals ((int) out.size(), 100);
            for (float v : out)
                expectWithinAbsoluteError (v, 0.8f, 1.0e-6f);
        }

        beginTest ("the transport position of the FIRST recorded block is kept, and only that one");
        {
            InputRecorder r;
            r.prepare (44100.0, 1.0);

            expectEquals (r.getStartTransportSample(), (std::int64_t) -1, "nothing recorded yet");

            r.start();
            feed (r, 0.5f, 1, 128, 88200);   // recording began two seconds into the loop
            feed (r, 0.5f, 1, 128, 88328);

            expectEquals (r.getStartTransportSample(), (std::int64_t) 88200,
                          "a later block overwrote the start position");

            r.stop();
            r.take();
            r.start();
            expectEquals (r.getStartTransportSample(), (std::int64_t) -1, "start() must re-arm cleanly");
        }

        beginTest ("a take that outruns the buffer stops, and says so, rather than growing");
        {
            InputRecorder r;
            r.prepare (1000.0, 0.5);         // 500 samples
            r.start();

            feed (r, 1.0f, 1, 400);
            expect (! r.isFull());
            expect (r.isArmed());

            feed (r, 1.0f, 1, 400);          // only 100 of these fit
            expect (r.isFull(), "it did not notice the end of the buffer");
            expect (! r.isArmed(), "a full recorder must disarm itself");
            expectEquals (r.samplesCaptured(), 500);

            feed (r, 1.0f, 1, 100);          // and it stays stopped
            expectEquals (r.samplesCaptured(), 500);

            expectEquals ((int) r.take().size(), 500);
        }

        beginTest ("take() resets, so the next recording starts from nothing");
        {
            InputRecorder r;
            r.prepare (44100.0, 1.0);

            r.start();
            feed (r, 0.3f, 1, 200);
            r.stop();
            expectEquals ((int) r.take().size(), 200);
            expectEquals (r.samplesCaptured(), 0);

            r.start();
            feed (r, 0.6f, 1, 50);
            const auto second = r.take();
            expectEquals ((int) second.size(), 50, "the second take carried the first one's samples");
            expectWithinAbsoluteError (second[0], 0.6f, 1.0e-6f);
        }

        beginTest ("stop() then take() collects everything, while a thread is still calling writeBlock");
        {
            // The race the class exists to prevent. An "audio thread" hammers writeBlock while
            // this one stops and collects; every sample it wrote before stop() must be present,
            // and nothing may be read while it is still writing. ThreadSanitizer checks the
            // second half of that claim; this test checks the first.
            InputRecorder r;
            r.prepare (48000.0, 2.0);

            std::atomic<bool> running { true };
            std::atomic<int>  blocksWritten { 0 };

            std::thread audio ([&]
            {
                std::int64_t transport = 0;
                while (running.load (std::memory_order_acquire))
                {
                    feed (r, 0.25f, 2, 64, transport);
                    transport += 64;
                    ++blocksWritten;
                }
            });

            r.start();
            juce::Thread::sleep (30);
            r.stop();

            const auto out = r.take();          // must not tear, must not deadlock
            running.store (false, std::memory_order_release);
            audio.join();

            expect (blocksWritten.load() > 0, "the audio thread never ran");
            expect (! out.empty(), "nothing was captured across the threads");
            expectEquals ((int) out.size() % 64, 0, "a block was torn in half");

            for (float v : out)
                expectWithinAbsoluteError (v, 0.25f, 1.0e-6f, "a sample was read while it was being written");
        }
    }
};

static InputRecorderTest inputRecorderTest;

} // namespace rollforge::tests
