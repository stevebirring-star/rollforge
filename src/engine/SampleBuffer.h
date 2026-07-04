#pragma once

// RollForge — SampleBuffer: immutable, reference-counted decoded audio.
//
// ENGINE LAYER RULE: like everything under src/engine, this must never include
// a JUCE GUI module. SampleBuffer depends only on juce_audio_basics (AudioBuffer)
// and juce_core (reference counting), so it can be reused verbatim inside a
// GUI-less VST3/AU wrapper later.
//
// OWNERSHIP / REAL-TIME RECLAMATION CONTRACT
// ------------------------------------------
// A SampleBuffer owns decoded PCM audio at its NATIVE sample rate and is
// IMMUTABLE once constructed. Because nothing ever mutates it, it can be shared
// across threads with no locking — the audio thread only ever READS it.
//
// Lifetime is managed by juce::ReferenceCountedObjectPtr. The single rule that
// keeps the audio thread real-time safe is:
//
//     *** The final delete of a SampleBuffer must NEVER run on the audio thread. ***
//
// Taking or releasing a reference is a lock-free atomic inc/dec (RT-safe). The
// hazard is only the delete that fires when a refcount reaches zero — a heap
// free, which is forbidden on the audio thread. We avoid it structurally:
//
//   * A Pad points at its current SampleBuffer via a Ptr (the message thread
//     owns the pad model).
//   * A playing Voice copies that Ptr at note-on and holds it for the whole
//     note (the incref is RT-safe).
//   * When a pad's sample is replaced (drag-drop, NEW KIT, device-SR rebuild),
//     the OLD buffer is handed to a SampleRetirementPool (message thread) BEFORE
//     the pad drops its own reference. The pool keeps one strong reference, so
//     even when the last Voice finishes and drops ITS reference on the audio
//     thread, the count goes 2 -> 1 (the pool still holds one) and never hits
//     zero — so no delete happens on the audio thread.
//   * The pool's sweep() runs on the MESSAGE THREAD and frees a retired buffer
//     only once its reference count has fallen to 1 (no Voice references it),
//     so the actual delete always happens off the audio thread.
//
// See SampleRetirementPool.h for the collection side of this contract.

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

/** Immutable, reference-counted block of decoded audio at its native rate.

    Not marked `final`: it derives from juce::ReferenceCountedObject and is left
    open so tests (and future variants) can subclass it. Instances must always
    be owned through SampleBuffer::Ptr — never on the stack or via a bare owning
    pointer.
*/
class SampleBuffer : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SampleBuffer>;

    /** Takes ownership of already-decoded audio at its native sample rate.
        @param audio       decoded PCM, moved in (may be empty).
        @param sampleRate  the audio's native sample rate in Hz (> 0).
        @param name        optional human label (e.g. the source file name). */
    SampleBuffer (juce::AudioBuffer<float>&& audio,
                  double sampleRate,
                  juce::String name = {});

    ~SampleBuffer() override;

    //==============================================================================
    // Immutable accessors — safe from any thread, including the audio thread.
    const juce::AudioBuffer<float>& getAudio() const noexcept { return audio; }
    int    getNumChannels() const noexcept { return audio.getNumChannels(); }
    int    getNumSamples()  const noexcept { return audio.getNumSamples(); }
    double getSampleRate()  const noexcept { return sampleRate; }
    const juce::String& getName() const noexcept { return name; }

    /** Reads a single sample. The channel is clamped into range, so a mono
        buffer can be read on any output channel. Returns 0 for an empty buffer
        or an out-of-range sample index. RT-safe. */
    float getSample (int channel, int sampleIndex) const noexcept;

private:
    const juce::AudioBuffer<float> audio;
    const double                   sampleRate;
    const juce::String             name;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleBuffer)
};

} // namespace rollforge
