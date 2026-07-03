#pragma once

// RollForge audio engine.
//
// ENGINE LAYER RULE: this header (and everything under src/engine) must never
// include a JUCE GUI module (juce_gui_basics, juce_gui_extra, juce_graphics).
// The engine talks only to audio/basics so it can later be reused verbatim
// inside a VST3/AU wrapper with no GUI present.

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace rollforge
{

/** Owns the audio device and renders audio on the real-time thread.

    Phase 0 responsibility is deliberately tiny: initialise a device and, on
    request from the UI thread, play one short sine "blip". The blip is the
    smoke test proving the whole audio path (device -> callback -> speakers)
    is alive before any of the real sequencer machinery arrives in Phase 1+.

    Real-time safety: the audio callback performs no allocation, locking or
    logging. The only cross-thread channel is a single lock-free atomic flag
    set by the UI thread and consumed (exchanged) by the audio thread.
*/
class AudioEngine final : private juce::AudioIODeviceCallback
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

    /** Requests a blip from any thread. Real-time safe. */
    void triggerBlip() noexcept { blipRequested.store (true, std::memory_order_release); }

    /** True if a device is currently open and running. UI-thread use only. */
    bool isAudioRunning() const noexcept { return audioRunning.load (std::memory_order_acquire); }

    /** Exposed so the UI can host an AudioDeviceSelectorComponent. The engine
        keeps ownership; the UI only reads/edits the shared device manager. */
    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

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
    juce::AudioDeviceManager deviceManager;

    // Cross-thread flags.
    std::atomic<bool> blipRequested { false };
    std::atomic<bool> audioRunning  { false };

    // Blip voice state — touched only on the audio thread once running.
    double sampleRate          = 44100.0;
    double phase               = 0.0;   // radians
    double phaseIncrement      = 0.0;   // radians / sample
    int    blipSamplesRemaining = 0;
    int    blipLengthSamples    = 0;

    static constexpr double blipFrequencyHz = 880.0;   // A5
    static constexpr double blipSeconds     = 0.12;    // 120 ms
    static constexpr float  blipGain        = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace rollforge
