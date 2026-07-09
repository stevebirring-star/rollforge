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

    // Enable every available MIDI input and listen for note-ons -> pad triggers.
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        deviceManager.addMidiInputDeviceCallback (device.identifier, this);
        enabledMidiInputs.add (device.identifier);
    }
}

void AudioEngine::shutdown()
{
    for (const auto& id : enabledMidiInputs)
        deviceManager.removeMidiInputDeviceCallback (id, this);
    enabledMidiInputs.clear();

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
    // to (re)allocate rate-dependent state here.
    drumEngine.prepare (sampleRate, blockSize);
    sequencer.prepare (sampleRate);
    masterBus.prepare (sampleRate, blockSize);
    outputMeter.prepare (sampleRate);

    audioRunning.store (true, std::memory_order_release);
}

void AudioEngine::audioDeviceStopped()
{
    audioRunning.store (false, std::memory_order_release);
    outputMeter.reset();   // the needles fall to rest rather than freezing mid-scale
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

    // Flush denormals for the whole callback. Every feedback path in the signal chain
    // decays towards zero and then sits there: the two plate reverbs' comb lines, the
    // limiter's and compressor's envelope followers, the per-voice tone filter. A
    // denormal multiply costs orders of magnitude more than a normal one on x86, so an
    // idle-but-running reverb is exactly the thing that turns into an audio dropout.
    const juce::ScopedNoDenormals noDenormals;

    // Wrap the driver's output channels (JUCE guarantees non-null output pointers
    // for [0, numOutputChannels)), start from silence, then let the DrumEngine
    // render additively. All RT-safe: no alloc/lock/IO on this thread.
    juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
    output.clear();
    // The sequencer drains the engine's UI/MIDI queues, fires sequenced triggers
    // at sample-accurate offsets, and renders the block into `output`.
    sequencer.process (drumEngine, output);

    // Master chain (future macro FX) + always-on brickwall limiter.
    masterBus.process (output);

    // Meter what actually leaves the app: after the limiter, not before it.
    for (int ch = 0; ch < juce::jmin (2, numOutputChannels); ++ch)
        outputMeter.processBlock (ch, output.getReadPointer (ch), numSamples);
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // Runs on a MIDI-input thread; route note-ons to pad triggers (RT-safe queue).
    if (message.isNoteOn())
    {
        const int pad = midiNoteToPad (message.getNoteNumber());
        if (pad >= 0)
            drumEngine.pushMidiTrigger (pad, message.getFloatVelocity());
    }
}

} // namespace rollforge
