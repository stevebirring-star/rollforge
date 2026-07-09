#include "engine/MidiCaptureQueue.h"

#include <algorithm>

namespace rollforge
{

MidiCaptureQueue::MidiCaptureQueue (int capacity)
{
    notes.reserve ((std::size_t) std::max (1, capacity));
}

void MidiCaptureQueue::push (const Note& note) noexcept
{
    const juce::SpinLock::ScopedLockType sl (lock);

    // reserve() in the constructor is what makes this allocation-free; push_back past the
    // reserved capacity would allocate on a MIDI thread, so it is refused instead.
    if (notes.size() >= notes.capacity())
    {
        ++drops;
        return;
    }

    notes.push_back (note);
}

MidiCaptureQueue::Batch MidiCaptureQueue::take()
{
    // Reserve BEFORE the lock. Swapping the buffer out would leave the queue with no capacity
    // and the next push() would allocate on a MIDI thread; allocating under the lock would make
    // a MIDI thread spin through a malloc. Copying a few hundred notes costs neither.
    Batch batch;
    batch.notes.reserve ((std::size_t) capacity());

    const juce::SpinLock::ScopedLockType sl (lock);
    batch.notes.assign (notes.begin(), notes.end());
    batch.dropped = drops;

    notes.clear();   // keeps the capacity
    drops = 0;
    return batch;
}

void MidiCaptureQueue::clear() noexcept
{
    const juce::SpinLock::ScopedLockType sl (lock);
    notes.clear();   // keeps the capacity
    drops = 0;
}

int MidiCaptureQueue::size() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return (int) notes.size();
}

int MidiCaptureQueue::capacity() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return (int) notes.capacity();
}

} // namespace rollforge
