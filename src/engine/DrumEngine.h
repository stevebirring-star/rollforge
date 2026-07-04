#pragma once

// RollForge — DrumEngine: the audio-thread sound source that replaces the
// Phase-0 blip. It receives commands from producer threads through a lock-free
// CommandQueue and renders audio (through a VoicePool) into a buffer supplied by
// the host callback.
//
// DEVICE-AGNOSTIC BY DESIGN: DrumEngine knows nothing about AudioDeviceManager.
// AudioEngine owns the device and drives DrumEngine via prepare()/process(), so
// DrumEngine + VoicePool are fully unit-testable headless, with no audio hardware.
//
// THREADING:
//   * pushTrigger() / pushCommand() — PRODUCER (message thread; later also MIDI).
//   * prepare()                     — audio/device thread, at stream start
//                                     (allocation permitted; not in process()).
//   * process()                     — AUDIO THREAD. No allocation/locking/IO.
//
// PHASE-1 STATUS: the command FIFO + VoicePool are in place and polyphonic. Every
// trigger currently plays a synthesised INTERIM blip with default voice params
// and no choke group, because there is no loaded Kit yet — the Pad -> sample +
// Voice::Parameters + choke-group mapping is wired when StarterKit (7/9) provides
// real per-pad sounds.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/CommandQueue.h"
#include "engine/SampleBuffer.h"
#include "engine/VoicePool.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

class DrumEngine final
{
public:
    explicit DrumEngine (int commandCapacity = 1024, int numVoices = 64);

    //==============================================================================
    // Producer-thread API (message / MIDI thread).

    /** Queues a pad trigger. Returns false if the command queue is full. */
    bool pushTrigger (int padIndex, float velocity = 1.0f) noexcept;

    /** Queues a raw command. Returns false if the command queue is full. */
    bool pushCommand (const EngineCommand& command) noexcept;

    //==============================================================================
    // Audio-thread API.

    /** Prepares for playback at the given device rate / block size, (re)building
        the interim sound and preparing the voice pool. Call from the device's
        aboutToStart (allocation permitted); not real-time safe. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Drains queued commands, then renders (ADDITIVELY) into `buffer`. The
        caller owns clearing the buffer first. RT-safe: no alloc/lock/IO/logging. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    //==============================================================================
    double getSampleRate()   const noexcept { return sampleRate; }
    int    getNumActiveVoices() const noexcept { return pool.getNumActive(); }

private:
    void handleCommand (const EngineCommand& command) noexcept;

    CommandQueue commands;
    VoicePool    pool;

    double sampleRate = 44100.0;

    // Interim per-trigger sound, played through the pool until StarterKit (7/9)
    // installs real per-pad samples.
    SampleBuffer::Ptr interimSound;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEngine)
};

} // namespace rollforge
