#pragma once

// RollForge — CommandQueue: a single-producer / single-consumer, lock-free queue
// of EngineCommands built on juce::AbstractFifo.
//
// THREADING CONTRACT (strict SPSC):
//   * push()  — called by exactly ONE producer thread (e.g. the message thread,
//               or later a MIDI-input thread). Never blocks, never allocates.
//   * drain() — called by exactly ONE consumer thread (the audio thread). Never
//               blocks, never allocates; runs the supplied callable once per
//               queued command, in FIFO order.
//
// juce::AbstractFifo is SPSC only, so use one CommandQueue PER PRODUCER THREAD
// (keyboard and MIDI get their own instances in a later commit). The backing
// storage is preallocated in the constructor, so neither side ever touches the
// heap.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/EngineCommand.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace rollforge
{

class CommandQueue final
{
public:
    /** @param capacity  maximum number of pending commands (clamped to >= 1). */
    explicit CommandQueue (int capacity = 1024)
        : fifo (juce::jmax (1, capacity)),
          storage ((size_t) juce::jmax (1, capacity))
    {
    }

    /** Enqueues a command. PRODUCER THREAD ONLY. Returns false (dropping the
        command) if the queue is full. Lock-free and allocation-free. */
    bool push (const EngineCommand& command) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            storage[(size_t) start1] = command;
            fifo.finishedWrite (1);
            return true;
        }

        return false;   // full
    }

    /** Runs `fn(const EngineCommand&)` for every queued command, oldest first,
        and removes them. CONSUMER THREAD ONLY. Lock-free and allocation-free
        (provided `fn` is). */
    template <typename Fn>
    void drain (Fn&& fn) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i) fn (storage[(size_t) (start1 + i)]);
        for (int i = 0; i < size2; ++i) fn (storage[(size_t) (start2 + i)]);

        fifo.finishedRead (size1 + size2);
    }

    /** Number of pending commands (approximate while the other thread runs). */
    int getNumReady() const noexcept { return fifo.getNumReady(); }

    /** Free capacity right now. */
    int getFreeSpace() const noexcept { return fifo.getFreeSpace(); }

private:
    juce::AbstractFifo         fifo;
    std::vector<EngineCommand> storage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CommandQueue)
};

} // namespace rollforge
