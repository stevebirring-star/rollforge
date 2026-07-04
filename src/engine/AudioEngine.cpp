#include "engine/AudioEngine.h"

namespace rollforge
{

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    // Zero inputs, up to two outputs. Returns a non-empty error string on
    // failure (e.g. no audio hardware) — we tolerate that so the app still
    // opens and can be pointed at a device from the settings dialog.
    const juce::String error = deviceManager.initialiseWithDefaultDevices (0, 2);
    if (error.isNotEmpty())
    {
        // Not fatal. Leave the engine idle; UI reflects isAudioRunning() == false.
        juce::Logger::writeToLog ("RollForge audio init: " + error);
    }

    deviceManager.addAudioCallback (this);
}

void AudioEngine::shutdown()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    double sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    const int blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    // aboutToStart is bracketed around the callback stream by JUCE, so it is safe
    // to (re)allocate the DrumEngine's rate-dependent state here.
    drumEngine.prepare (sampleRate, blockSize);

    audioRunning.store (true, std::memory_order_release);
}

void AudioEngine::audioDeviceStopped()
{
    audioRunning.store (false, std::memory_order_release);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* /*inputChannelData*/,
                                                    int /*numInputChannels*/,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (numOutputChannels <= 0 || numSamples <= 0)
        return;

    // Wrap the driver's output channels (JUCE guarantees non-null output pointers
    // for [0, numOutputChannels)), start from silence, then let the DrumEngine
    // render additively. All RT-safe: no alloc/lock/IO on this thread.
    juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
    output.clear();
    drumEngine.process (output);
}

} // namespace rollforge
