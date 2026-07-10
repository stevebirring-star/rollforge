#pragma once

// RollForge — MidiCaptureQueue: notes played on a controller, waiting to be quantised.
//
// A MIDI note-on arrives on a MIDI-input thread -- one per device, so genuinely more than one
// producer -- and has to reach the message thread, which is the only thread allowed to touch
// the pattern or the undo history. It cannot wait there: the pad has already sounded, and what
// is queued here is only the RECORD of the note, timestamped against the transport.
//
// A SpinLock, not a lock-free FIFO. juce::AbstractFifo is single-producer, and this has
// several; the alternative is a queue per device, which is more moving parts than a lock held
// for the time it takes to append four words. A MIDI thread is not the audio thread -- it may
// block for microseconds, and JUCE's own MIDI collectors do exactly this.
//
// BOUNDED, AND IT DROPS. push() never allocates: past capacity it discards the note and counts
// it. Someone drumming through a dropped-frames stall should lose the tail of their fill, not
// the audio device.
//
// The drops come back WITH the notes, from one call under one lock. Reading them separately
// loses every note dropped between the two calls, which is exactly when notes get dropped.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace rollforge
{

class MidiCaptureQueue
{
public:
    struct Note
    {
        int          pad             = 0;
        float        velocity        = 1.0f;
        std::int64_t transportSample = 0;   // where the loop was when it was played
    };

    /** Sized once, up front. 512 notes is about twenty seconds of frantic drumming. */
    explicit MidiCaptureQueue (int capacity = 512);

    /** ANY MIDI THREAD. Appends, or counts a drop when full. Never allocates. */
    void push (const Note& note) noexcept;

    struct Batch
    {
        std::vector<Note> notes;
        int               dropped = 0;   // notes lost since the last take() or clear()
    };

    /** MESSAGE THREAD. Moves everything out, leaving the queue empty. The drop count comes
        with it: a caller that reads them separately cannot account for its own notes. */
    Batch take();

    /** MESSAGE THREAD. Throws away anything queued, and forgets the drops. Used when a
        capture ends: notes played after REC was released are not part of the take. */
    void clear() noexcept;

    int size() const noexcept;
    int capacity() const noexcept;

private:
    mutable juce::SpinLock lock;
    std::vector<Note>      notes;
    int                    drops = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiCaptureQueue)
};

} // namespace rollforge
