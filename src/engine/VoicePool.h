#pragma once

// RollForge — VoicePool: a fixed pool of Voices with voice-stealing and choke
// groups. This is the polyphony layer DrumEngine renders through.
//
// VOICE-STEAL POLICY (deterministic, headless-testable):
//   1. reuse any idle voice;
//   2. otherwise steal the QUIETEST (lowest Voice::getLevel());
//   3. tie-break on the OLDEST (lowest trigger counter);
//   4. final tie-break on the lowest slot index.
// "Quietest" is the tracked envelope*gain*velocity level, never live audio RMS,
// so stealing is deterministic and testable with no audio device.
//
// CHOKE GROUPS: a non-zero choke group means "mutually exclusive" — triggering a
// voice in group G first stops every active voice in group G (e.g. a closed hat
// chokes the open hat). Group 0 = no choke. (The convention mirrors
// model/Pad.h's noChokeGroup, kept as a literal here so the engine stays free of
// the model layer.)
//
// THREADING: prepare() at stream start; trigger()/renderAdditive()/reset() on the
// AUDIO THREAD — all allocation-free (the voices + bookkeeping are preallocated
// in the constructor). No locks/IO.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/Voice.h"

#include <memory>
#include <vector>

namespace rollforge
{

class VoicePool final
{
public:
    /** @param numVoices  pool size (clamped to >= 1). Spec default is 64. */
    explicit VoicePool (int numVoices = 64);

    /** Prepares every voice for the device rate and resets the pool to idle. */
    void prepare (double deviceSampleRate);

    /** Triggers a note: applies the choke group, allocates a voice (stealing if
        needed), and starts it. Null sample is ignored. AUDIO THREAD. */
    void trigger (SampleBuffer::Ptr sample,
                  const Voice::Parameters& params,
                  float velocity,
                  int chokeGroup = 0) noexcept;

    /** Sums every active voice ADDITIVELY into `buffer`. AUDIO THREAD. */
    void renderAdditive (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    /** Hard-stops every voice. */
    void reset() noexcept;

    //==============================================================================
    int getNumVoices() const noexcept { return numVoices; }
    int getNumActive() const noexcept;

    // Per-voice inspection (metering / tests).
    bool        isVoiceActive (int index) const noexcept;
    float       getVoiceLevel (int index) const noexcept;
    juce::int64 getVoiceAge   (int index) const noexcept;

private:
    int selectVoice() const noexcept;

    std::unique_ptr<Voice[]> voices;
    std::vector<int>         chokeGroups;   // per voice; 0 when idle / no group
    std::vector<juce::int64> ages;          // trigger counter at last start()

    int         numVoices = 0;
    juce::int64 nextAge   = 0;
    double      deviceSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicePool)
};

} // namespace rollforge
