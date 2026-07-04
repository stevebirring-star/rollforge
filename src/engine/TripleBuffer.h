#pragma once

// RollForge — TripleBuffer: a lock-free single-producer / single-consumer triple
// buffer that always hands the reader the LATEST value the writer published.
//
// Used to pass a whole Pattern snapshot from the message thread (writer) to the
// audio thread (reader): the reader gets a consistent, immutable snapshot per
// block without ever locking or allocating, and the writer can publish as often
// as it likes without stalling. Three buffers guarantee the writer and reader
// never touch the same buffer at once.
//
// THREADING: exactly one writer thread (writeBuffer()/publish()) and one reader
// thread (read()). Neither call allocates or locks — the buffers are members.
//
// ENGINE LAYER RULE: no JUCE includes (pure C++).

#include <atomic>

namespace rollforge
{

template <typename T>
class TripleBuffer
{
public:
    TripleBuffer() = default;

    TripleBuffer (const TripleBuffer&) = delete;
    TripleBuffer& operator= (const TripleBuffer&) = delete;

    /** WRITER thread: the buffer to fill before calling publish(). Note that each
        publish() hands this buffer off, so the next writeBuffer() is a different
        (recycled) buffer — fill it completely each time. */
    T& writeBuffer() noexcept { return buffers[writeIndex]; }

    /** WRITER thread: publishes the current writeBuffer() as the latest value. */
    void publish() noexcept
    {
        writeIndex = ready.exchange (writeIndex, std::memory_order_acq_rel);
        hasNew.store (true, std::memory_order_release);
    }

    /** READER thread: returns the latest published value, or the previously-read
        one if nothing new has been published. Real-time safe. */
    const T& read() noexcept
    {
        if (hasNew.exchange (false, std::memory_order_acquire))
            readIndex = ready.exchange (readIndex, std::memory_order_acq_rel);

        return buffers[readIndex];
    }

private:
    // The three indices {writeIndex, readIndex, ready} are always a permutation
    // of {0,1,2}; `ready` is the shared hand-off slot.
    T buffers[3] {};
    int writeIndex = 0;
    int readIndex  = 1;
    std::atomic<int>  ready  { 2 };
    std::atomic<bool> hasNew { false };
};

} // namespace rollforge
