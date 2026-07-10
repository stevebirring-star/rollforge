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
#include "engine/fx/Space.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
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

    /** As above, but with up to maxPadLayers samples the pad picks between (round-robin,
        or by velocity). Same lifetime + retirement contract for EVERY layer. Null layers
        are skipped; passing a single layer is identical to pushSetPad. */
    bool pushSetPadLayers (int padIndex,
                           const SampleBuffer::Ptr* layers,
                           int numLayers,
                           LayerMode layerMode,
                           const VoiceParameters& params,
                           int chokeGroup) noexcept;

    /** Queues a trigger from a MIDI-input thread. Uses a SEPARATE queue from the
        message-thread commands; because multiple MIDI devices deliver on multiple
        threads, the producer side is guarded by a spin lock so it stays a valid
        single-consumer FIFO for the (lock-free) audio thread. Returns false if
        the MIDI queue is full. */
    bool pushMidiTrigger (int padIndex, float velocity = 1.0f) noexcept;

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
    // Fine-grained audio-thread API, used by the Sequencer to render a block in
    // segments split at sample-accurate trigger offsets. All audio-thread only.

    /** Applies queued UI + MIDI commands (their triggers land at the call site). */
    void drainCommands() noexcept;

    /** Triggers a pad immediately from the audio thread (no queue). Plays the
        pad's configured sample/params/choke, or the interim blip if unconfigured.
        `pitchOffsetSemitones` is added to the pad's pitch (used by rolls).
        `sampleLock` >= 0 forces that layer instead of the pad's own choice — the
        sequencer passes each Step's sampleLock, so a step can pin one alternate. */
    void triggerPadNow (int padIndex, float velocity, float pitchOffsetSemitones = 0.0f,
                        int sampleLock = -1) noexcept;

    /** Renders the voice pool ADDITIVELY into `buffer[startSample, startSample+numSamples)`,
        accumulating each voice's reverb send into the engine's send buffer. */
    void renderInto (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    /** Restricts what renderInto() CAPTURES to one pad, without changing what it PLAYS:
        every pad still triggers, chokes and steals voices exactly as in the full mix, but
        only `padIndex`'s voices reach the output and the reverb send. `-1` (the default,
        and what prepare() restores) captures every pad.

        This exists for per-pad stem export. Stripping the other pads out of the Pattern
        instead -- the obvious implementation -- silently breaks any choke group: nothing
        is left to choke the open hat, so its stem rings on past where the mix cuts it.

        OFFLINE ONLY. Every offline render owns a private DrumEngine, so this is a plain
        int, not an atomic. Do not call it on an engine a device callback is driving. */
    void setCapturePad (int padIndex) noexcept { capturePad = padIndex; }

    /** Reverberates this block's accumulated per-pad sends and ADDS the wet to `buffer`,
        then clears the send buffer. Call ONCE per block, after the block's renderInto()
        segment(s) — the Sequencer renders a block in many segments, all feeding one send.

        The send reverb lives here rather than on the MasterBus because only the engine
        can see individual pads. It is therefore part of the instrument, applied before
        the master strip and included in per-pad stems — which is exactly what keeps the
        stems summing to the mix (Space is linear). */
    void applySendReturn (juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    /** Snapshots current pad output levels into the meter atomics for the UI. Call
        ONCE per audio block, after the block's renderInto() segment(s): the Sequencer
        renders a block in many segments, and publishing per-segment is wasted work. */
    void publishPadMeters() noexcept;

    //==============================================================================
    double getSampleRate()      const noexcept { return sampleRate; }
    int    getNumPads()         const noexcept { return (int) pads.size(); }
    int    getNumActiveVoices() const noexcept { return pool.getNumActive(); }

    /** Latest published output level (0..~1) for a pad, for UI level meters.
        Written by the audio thread each render, read by the UI timer (relaxed
        atomic — a benign meter race). Out-of-range pads read 0. */
    float getPadLevel (int padIndex) const noexcept
    {
        return (padIndex >= 0 && padIndex < maxMeterPads)
                   ? padMeter[(size_t) padIndex].load (std::memory_order_relaxed)
                   : 0.0f;
    }

    //==============================================================================
    // Per-pad mute / solo. Message thread sets; the audio thread (Sequencer) and the
    // UI read. Gating applies to SEQUENCED triggers only — a manual pad audition
    // always sounds, so you can still hear a muted pad when you click it.
    void setPadMuted  (int padIndex, bool shouldBeMuted) noexcept;
    void setPadSoloed (int padIndex, bool shouldBeSoloed) noexcept;
    bool isPadMuted   (int padIndex) const noexcept;
    bool isPadSoloed  (int padIndex) const noexcept;
    /** True if the pad should sound under the current mute/solo state: if ANY pad is
        soloed, only soloed pads are audible; otherwise every non-muted pad is. */
    bool isPadAudible (int padIndex) const noexcept;

private:
    static constexpr int maxMeterPads = 16;

    struct PadSlot
    {
        std::array<SampleBuffer::Ptr, maxPadLayers> layers;
        int               numLayers  = 0;
        int               layerMode  = (int) LayerMode::roundRobin;
        VoiceParameters   params;
        int               chokeGroup = 0;

        // Round-robin cursor. Touched ONLY from the audio thread (triggerPadNow), so a
        // plain int is correct and cheapest — unlike padMuted/padSoloed, which the
        // message thread writes. prepare() zeroes it, which is what keeps an offline
        // render reproducible; setPad deliberately leaves it alone, so turning a knob
        // mid-pattern doesn't restart the cycle.
        int               roundRobin = 0;
    };

    /** Which layer a hit should play. Audio thread only; advances the round-robin. */
    int pickLayer (PadSlot& slot, float velocity, int sampleLock) noexcept;

    void handleCommand (const EngineCommand& command) noexcept;

    CommandQueue   commands;        // producer: message thread (UI + keyboard + setPad)
    CommandQueue   midiCommands;    // producer: MIDI-input thread(s), guarded below
    juce::SpinLock midiProducerLock;

    VoicePool            pool;
    std::vector<PadSlot> pads;

    double sampleRate = 44100.0;

    // -1 = capture every pad (live playback + mixdown). >= 0 = capture only that pad
    // (stem export). See setCapturePad(). Audio-thread read; offline-only write.
    int capturePad = -1;

    // Fallback sound for a pad with no sample yet (played until a Kit is installed).
    SampleBuffer::Ptr interimSound;

    // Reverb send bus. Voices accumulate into sendBuffer during renderInto(); one
    // applySendReturn() per block reverberates it into the output and clears it.
    // `anySendActive` is recomputed on setPad (audio thread) and gates the reverb, with
    // a tail so the last hit's decay isn't cut off when the sends go to zero.
    Space                    sendReverb;
    juce::AudioBuffer<float> sendBuffer;
    bool                     anySendActive   = false;
    int                      sendTailSamples = 0;

    // Per-pad output level for UI meters: audio thread publishes, UI timer reads.
    std::array<std::atomic<float>, maxMeterPads> padMeter {};

    // Per-pad mute/solo: message thread writes, audio thread + UI read (relaxed).
    // soloCount tracks how many pads are soloed so isPadAudible is a cheap read.
    std::array<std::atomic<bool>, maxMeterPads> padMuted  {};
    std::array<std::atomic<bool>, maxMeterPads> padSoloed {};
    std::atomic<int>                            soloCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEngine)
};

} // namespace rollforge
