#pragma once

// RollForge — Voice: a single playing sample voice.
//
// Plays one SampleBuffer with per-voice resampling (samples are stored at their
// NATIVE rate; the Voice converts to the device rate and applies pitch), a
// linear interpolator, an AR amplitude envelope, equal-power pan and optional
// reverse. It is a ONE-SHOT: it plays to the end of the sample and goes idle
// (drums have no note-off in Phase 1). Polyphony, voice-stealing and choke
// groups live in VoicePool (5/9).
//
// RT-SAFETY: prepare() runs at stream start (no rendering). start(), stop() and
// renderAdditive() run on the AUDIO THREAD and never allocate, lock, or do IO.
// The Voice holds a strong SampleBuffer::Ptr for the whole note; per the
// retirement contract (SampleBuffer.h) dropping it can only decrement, never
// delete, on the audio thread (a pad or the retirement pool always holds a ref).
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/SampleBuffer.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

class Voice final
{
public:
    /** Per-note parameters. DrumEngine maps a Pad onto this (5/9); keeping Voice
        free of the model type keeps it independently testable. */
    struct Parameters
    {
        float gain           = 1.0f;   // linear
        float pan            = 0.0f;   // -1 = left .. +1 = right
        float pitchSemitones = 0.0f;   // -12 .. +12
        float attackMs       = 0.0f;   // >= 0
        float releaseMs      = 0.0f;   // >= 0
        bool  reverse        = false;
    };

    Voice() = default;

    /** Sets the device sample rate and resets to idle. Call before playback. */
    void prepare (double deviceSampleRate) noexcept;

    /** Begins playing `sample` with `params` at `velocity` (0..1); restarts if
        already active (a Voice is monophonic). Ignored if the sample is
        null/empty (the voice then idles). Audio-thread safe. */
    void start (SampleBuffer::Ptr sample, const Parameters& params, float velocity) noexcept;

    /** Immediately silences and idles the voice (hard stop; used for choke). */
    void stop() noexcept;

    bool isActive() const noexcept { return active; }

    /** Current output level (envelope * gain * velocity), 0 when idle. Used by
        VoicePool's "steal the quietest" policy (5/9). */
    float getLevel() const noexcept { return active ? lastEnv * levelGain : 0.0f; }

    /** Renders `numSamples` frames ADDITIVELY into `buffer` starting at
        `startSample`. The caller owns clearing the buffer. */
    void renderAdditive (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    float envelopeAt (int frame) const noexcept;
    float readMono (double sourcePos) const noexcept;

    double deviceSampleRate = 44100.0;

    SampleBuffer::Ptr sample;
    bool   active = false;

    double sourcePos    = 0.0;
    double increment    = 1.0;   // signed source step per output frame (negative = reversed)
    int    framesTotal  = 0;
    int    framesPlayed = 0;

    int    attackFrames  = 0;
    int    releaseFrames = 0;

    float  levelGain = 1.0f;                 // gain * velocity
    float  leftGain = 1.0f, rightGain = 1.0f, monoGain = 1.0f;
    float  lastEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Voice)
};

} // namespace rollforge
