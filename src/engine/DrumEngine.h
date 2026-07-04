#pragma once

// RollForge — DrumEngine: the audio-thread sound source that replaces the
// Phase-0 blip. It receives commands from producer threads through a lock-free
// CommandQueue and renders audio into a buffer supplied by the host callback.
//
// DEVICE-AGNOSTIC BY DESIGN: DrumEngine knows nothing about AudioDeviceManager.
// AudioEngine owns the device and drives DrumEngine via prepare()/process(), so
// DrumEngine — and, from later commits, VoicePool — is fully unit-testable
// headless, with no audio hardware.
//
// THREADING:
//   * pushTrigger() / pushCommand() — PRODUCER (message thread; later also MIDI).
//   * prepare()                     — audio/device thread, at stream start
//                                     (allocation is allowed here, not in process).
//   * process()                     — AUDIO THREAD. No allocation, locking, IO or
//                                     logging.
//
// PHASE-1 STATUS: this commit establishes the seam + the command FIFO and plays
// a synthesised INTERIM blip per trigger, so the whole path (command -> audio
// out) is testable now. Real per-pad sample playback, polyphony, voice-stealing
// and choke groups arrive with Voice (4/9) and VoicePool (5/9), which replace
// the interim generator.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/CommandQueue.h"
#include "engine/SampleBuffer.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

class DrumEngine final
{
public:
    explicit DrumEngine (int commandCapacity = 1024);

    //==============================================================================
    // Producer-thread API (message / MIDI thread).

    /** Queues a pad trigger. Returns false if the command queue is full. */
    bool pushTrigger (int padIndex, float velocity = 1.0f) noexcept;

    /** Queues a raw command. Returns false if the command queue is full. */
    bool pushCommand (const EngineCommand& command) noexcept;

    //==============================================================================
    // Audio-thread API.

    /** Prepares for playback at the given device rate / block size, and (re)builds
        the interim voice's synth buffer at that rate. Call from the device's
        aboutToStart (allocation permitted); not real-time safe. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Drains queued commands, then renders (ADDITIVELY) into `buffer`. The
        caller owns clearing the buffer first. RT-safe: no alloc/lock/IO/logging. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    //==============================================================================
    double getSampleRate() const noexcept { return sampleRate; }

private:
    void handleCommand (const EngineCommand& command) noexcept;
    void renderInterimVoice (juce::AudioBuffer<float>& buffer) noexcept;

    CommandQueue commands;

    double sampleRate = 44100.0;

    // --- Interim monophonic one-shot voice (placeholder until VoicePool, 5/9) --
    SampleBuffer::Ptr interimSound;   // synthesised blip at the current device rate
    int   interimPos  = -1;           // -1 = idle; otherwise next sample to play
    float interimGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEngine)
};

} // namespace rollforge
