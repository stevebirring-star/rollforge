#pragma once

// RollForge audio engine.
//
// ENGINE LAYER RULE: this header (and everything under src/engine) must never
// include a JUCE GUI module (juce_gui_basics, juce_gui_extra, juce_graphics).
// The engine talks only to audio/basics so it can later be reused verbatim
// inside a VST3/AU wrapper with no GUI present.
//
// AudioEngine owns the audio device and the real-time render callback, and
// drives a DrumEngine (the device-agnostic sound source) each block. The
// Phase-0 inline sine "blip" has been replaced by DrumEngine + a lock-free
// command queue: the UI now sends triggers as commands rather than toggling an
// atomic flag.

#include "engine/DrumEngine.h"
#include "engine/MasterBus.h"
#include "engine/PadMapping.h"
#include "engine/Sequencer.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace rollforge
{

/** Owns the audio device and renders audio on the real-time thread by driving a
    DrumEngine.

    Real-time safety: the audio callback performs no allocation, locking or
    logging — it clears the output, then asks the DrumEngine to render. Triggers
    cross from the UI thread through the DrumEngine's lock-free command queue.
*/
class AudioEngine final : private juce::AudioIODeviceCallback,
                          private juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Opens the default output device and starts the audio callback.
        Safe to call when no device exists (e.g. headless CI): it simply
        leaves the engine idle and never throws. */
    void initialise();

    /** Stops the callback and closes the device. Idempotent. */
    void shutdown();

    /** Triggers pad 0 (the interim blip) from any thread. Real-time safe.
        Kept named `triggerBlip` so the Phase-0 UI button / space bar keep
        working; general pad triggering is `triggerPad`. */
    void triggerBlip() noexcept { drumEngine.pushTrigger (0, 1.0f); }

    /** Queues a pad trigger from any (producer) thread. Real-time safe. */
    void triggerPad (int padIndex, float velocity = 1.0f) noexcept
    {
        drumEngine.pushTrigger (padIndex, velocity);
    }

    /** True if a device is currently open and running. UI-thread use only. */
    bool isAudioRunning() const noexcept { return audioRunning.load (std::memory_order_acquire); }

    /** Exposed so the UI can host an AudioDeviceSelectorComponent. The engine
        keeps ownership; the UI only reads/edits the shared device manager. */
    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    /** Exposed so the app can install a Kit into the DrumEngine (KitInstaller). */
    DrumEngine& getDrumEngine() noexcept { return drumEngine; }

    /** Exposed so the app/transport UI can drive the sequencer (pattern, play,
        tempo). All of its control methods are message-thread safe. */
    Sequencer& getSequencer() noexcept { return sequencer; }

private:
    //==============================================================================
    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==============================================================================
    // juce::MidiInputCallback — runs on a MIDI-input thread. Maps note-ons to pad
    // triggers via the MIDI command queue (no mapping UI in Phase 1).
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    //==============================================================================
    juce::AudioDeviceManager deviceManager;
    DrumEngine               drumEngine;
    Sequencer                sequencer;
    MasterBus                masterBus;

    juce::StringArray enabledMidiInputs;   // device ids we registered a callback on
    std::atomic<bool> audioRunning { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace rollforge
