#pragma once

// RollForge — SampleRetirementPool: message-thread-owned reclamation of
// SampleBuffers so their final delete never runs on the audio thread.
//
// See SampleBuffer.h for the full ownership / real-time contract. In short:
// when a pad's sample is swapped out while voices may still be playing the old
// buffer, that old buffer is retire()d here. This pool keeps one strong
// reference, so the last Voice releasing its reference on the audio thread can
// only drop the count to 1 (never 0) — no delete on the audio thread. sweep(),
// called periodically on the MESSAGE THREAD, then frees any retired buffer whose
// reference count has fallen to 1 (only this pool holds it), performing the
// actual delete off the audio thread.
//
// THREADING: retire(), sweep() and getNumRetired() are MESSAGE-THREAD ONLY. The
// audio thread never touches this pool — it merely holds Ptrs to buffers, which
// is what keeps them alive until it is finished with them.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/SampleBuffer.h"

namespace rollforge
{

class SampleRetirementPool final
{
public:
    SampleRetirementPool() = default;

    /** Hands a buffer over for deferred, RT-safe reclamation. Message thread
        only. A null buffer, or one already retired, is ignored. */
    void retire (SampleBuffer::Ptr buffer);

    /** Frees every retired buffer that no Voice still references — i.e. whose
        reference count is 1, meaning only this pool holds it. Message thread
        only. Returns the number of buffers freed by this call. */
    int sweep();

    /** How many buffers are currently awaiting reclamation. Message thread only. */
    int getNumRetired() const noexcept { return retired.size(); }

private:
    // The sole "retirement owner" reference to each buffer. Touched only on the
    // message thread, so no locking is required around it.
    juce::ReferenceCountedArray<SampleBuffer> retired;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleRetirementPool)
};

} // namespace rollforge
