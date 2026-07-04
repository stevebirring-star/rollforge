#pragma once

// RollForge — DrumEngine: the audio-thread sound source that replaces the
// Phase-0 blip. It receives commands from producer threads through a lock-free
// CommandQueue and renders audio (through a VoicePool) into a buffer supplied by
// the host callback.
//
// DEVICE-AGNOSTIC BY DESIGN: DrumEngine knows nothing about AudioDeviceManager
// or the model layer. AudioEngine owns the device and drives DrumEngine via
// prepare()/process(); the Kit -> pad mapping is done by a producer (see
// library/KitInstaller) that calls pushSetPad(). So DrumEngine + VoicePool are
// fully unit-testable headless, with no audio hardware and no model dependency.
//
// THREADING:
//   * pushTrigger()/pushCommand()/pushSetPad() — PRODUCER (message thread; MIDI).
//   * prepare()                                — audio/device thread, stream start
//                                                (allocation permitted; not process).
//   * process()                                — AUDIO THREAD. No alloc/lock/IO.
//
// A pad with a configured sample plays that sample with its params + choke group;
// a pad with no sample yet falls back to a synthesised interim blip so the app is
// never silent before a Kit is installed.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/CommandQueue.h"
#include "engine/SampleBuffer.h"
#include "engine/VoicePool.h"
#include "engine/VoiceParameters.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace rollforge
{

class DrumEngine final
{
public:
    explicit DrumEngine (int commandCapacity = 1024, int numVoices = 64, int numPads = 16);

    //==============================================================================
    // Producer-thread API (message / MIDI thread).

    /** Queues a pad trigger. Returns false if the command queue is full. */
    bool pushTrigger (int padIndex, float velocity = 1.0f) noexcept;

    /** Queues a raw command. Returns false if the command queue is full. */
    bool pushCommand (const EngineCommand& command) noexcept;

    /** Queues a pad (re)configuration: sample + params + choke group. The caller
        MUST keep `sample` alive until the pad is replaced/cleared (e.g. by holding
        the Kit), and — when replacing a live sample — retire the previous one
        first (SampleBuffer.h). Returns false if the queue is full. */
    bool pushSetPad (int padIndex,
                     SampleBuffer::Ptr sample,
                     const VoiceParameters& params,
                     int chokeGroup) noexcept;

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
    double getSampleRate()      const noexcept { return sampleRate; }
    int    getNumPads()         const noexcept { return (int) pads.size(); }
    int    getNumActiveVoices() const noexcept { return pool.getNumActive(); }

private:
    struct PadSlot
    {
        SampleBuffer::Ptr sample;
        VoiceParameters   params;
        int               chokeGroup = 0;
    };

    void handleCommand (const EngineCommand& command) noexcept;

    CommandQueue         commands;
    VoicePool            pool;
    std::vector<PadSlot> pads;

    double sampleRate = 44100.0;

    // Fallback sound for a pad with no sample yet (played until a Kit is installed).
    SampleBuffer::Ptr interimSound;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEngine)
};

} // namespace rollforge
