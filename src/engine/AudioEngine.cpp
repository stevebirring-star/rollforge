#include "engine/AudioEngine.h"

#include <cmath>

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
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    phaseIncrement      = juce::MathConstants<double>::twoPi * blipFrequencyHz / sampleRate;
    blipLengthSamples   = (int) (sampleRate * blipSeconds);
    blipSamplesRemaining = 0;
    phase               = 0.0;

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
    // Always start from silence — RT-safe (no alloc/lock/IO).
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    // Consume a pending trigger: reset the one-shot blip voice.
    if (blipRequested.exchange (false, std::memory_order_acquire))
    {
        phase                = 0.0;
        blipSamplesRemaining = blipLengthSamples;
    }

    if (blipSamplesRemaining <= 0 || blipLengthSamples <= 0)
        return;

    for (int i = 0; i < numSamples && blipSamplesRemaining > 0; ++i)
    {
        // Quadratic fade-out envelope (env^2) for a click-free, snappy blip.
        const double env    = (double) blipSamplesRemaining / (double) blipLengthSamples;
        const float  value  = (float) (std::sin (phase) * env * env) * blipGain;

        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                outputChannelData[ch][i] = value;

        phase += phaseIncrement;
        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        --blipSamplesRemaining;
    }
}

} // namespace rollforge
