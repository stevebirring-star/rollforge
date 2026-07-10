#pragma once

// RollForge — InputRecorder: capture the audio input into a preallocated buffer.
//
// The whole class exists to get samples off the audio thread without allocating on it, and to
// hand them to the message thread without either one reading what the other is writing.
//
// THE HANDSHAKE. `armed` is what the audio thread checks; `writing` is what it holds while it
// is inside the buffer. stop() clears `armed`, then take() spins on `writing` -- for at most
// one audio block, microseconds -- before it reads. The release/acquire pair on `writing` is
// what makes the samples the audio thread wrote visible to the thread that reads them. Without
// it this is a data race that happens to work, which is the worst kind.
//
// It records MONO: the input is summed. A beatbox take has no stereo image worth keeping, and
// everything downstream (onset detection, features) wants one channel anyway.
//
// It never grows. prepare() sizes it once; a take that runs past the end simply stops, and
// isFull() says so. Reallocating under the audio thread is not a trade worth making for a
// recording nobody wanted to be that long.
//
// ENGINE LAYER RULE: no JUCE GUI. RT-safe: no alloc/lock/IO in writeBlock().

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace rollforge
{

class InputRecorder
{
public:
    // Declaring the copy constructor (below) suppresses the implicit default one.
    InputRecorder() = default;

    /** MESSAGE THREAD. Allocates. Safe to call again with a different rate. */
    void prepare (double sampleRate, double maxSeconds = 30.0);

    /** MESSAGE THREAD. Arms the recorder; the audio thread starts appending at its next block. */
    void start() noexcept;

    /** MESSAGE THREAD. Disarms. The samples stay until take() collects them. */
    void stop() noexcept;

    bool isArmed() const noexcept { return armed.load (std::memory_order_acquire); }
    bool isFull()  const noexcept { return full.load (std::memory_order_acquire); }
    int  samplesCaptured() const noexcept { return written.load (std::memory_order_acquire); }
    double getSampleRate() const noexcept { return sampleRate; }

    /** The transport position at the first block that was recorded, or -1 if nothing was.
        Capture needs it: the recording did not begin at the top of the loop. */
    std::int64_t getStartTransportSample() const noexcept
    {
        return startTransportSample.load (std::memory_order_acquire);
    }

    /** AUDIO THREAD. Sums `input` to mono and appends it. Does nothing unless armed. */
    void writeBlock (const float* const* input, int numChannels, int numSamples,
                     std::int64_t transportSampleAtBlockStart) noexcept;

    /** MESSAGE THREAD. Waits for any in-flight block, then moves the recording out and resets.
        Call stop() first, or you will collect a take that is still growing. */
    std::vector<float> take();

private:
    std::vector<float> buffer;
    double             sampleRate = 44100.0;

    std::atomic<bool> armed   { false };
    std::atomic<bool> writing { false };   // the audio thread is inside `buffer` right now
    std::atomic<bool> full    { false };
    std::atomic<int>  written { 0 };
    std::atomic<std::int64_t> startTransportSample { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputRecorder)
};

} // namespace rollforge
