#include "engine/DrumEngine.h"

#include <cmath>

namespace rollforge
{

namespace
{
    // A short synthesised "blip" — the INTERIM trigger sound until real per-pad
    // samples + VoicePool land. ~120 ms, 880 Hz, quadratic fade-out (click-free).
    SampleBuffer::Ptr makeInterimBlip (double sampleRate)
    {
        constexpr double freqHz  = 880.0;
        constexpr double seconds = 0.12;
        const int        n       = juce::jmax (1, (int) (sampleRate * seconds));

        juce::AudioBuffer<float> audio (1, n);
        auto* data = audio.getWritePointer (0);

        const double inc = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double env = (double) (n - i) / (double) n;   // 1 -> 0
            data[i] = (float) (std::sin (phase) * env * env);

            phase += inc;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        return new SampleBuffer (std::move (audio), sampleRate, "interim-blip");
    }
}

DrumEngine::DrumEngine (int commandCapacity)
    : commands (commandCapacity)
{
}

bool DrumEngine::pushTrigger (int padIndex, float velocity) noexcept
{
    return commands.push (EngineCommand::makeTrigger (padIndex, velocity));
}

bool DrumEngine::pushCommand (const EngineCommand& command) noexcept
{
    return commands.push (command);
}

void DrumEngine::prepare (double newSampleRate, int /*maxBlockSize*/)
{
    sampleRate   = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    interimSound = makeInterimBlip (sampleRate);
    interimPos   = -1;
    interimGain  = 0.0f;
}

void DrumEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Drain producer commands first, so a trigger applies from the block's top.
    commands.drain ([this] (const EngineCommand& c) { handleCommand (c); });

    renderInterimVoice (buffer);
}

void DrumEngine::handleCommand (const EngineCommand& command) noexcept
{
    switch (command.type)
    {
        case CommandType::triggerPad:
            // Interim: (re)start the single one-shot voice. Real per-pad
            // polyphonic voices + stealing/choke arrive with VoicePool (5/9).
            interimPos  = 0;
            interimGain = juce::jlimit (0.0f, 1.0f, command.velocity);
            break;

        case CommandType::none:
        default:
            break;
    }
}

void DrumEngine::renderInterimVoice (juce::AudioBuffer<float>& buffer) noexcept
{
    if (interimPos < 0 || interimSound == nullptr)
        return;

    const auto* src         = interimSound->getAudio().getReadPointer (0);
    const int   length      = interimSound->getNumSamples();
    const int   numChannels = buffer.getNumChannels();
    const int   numSamples  = buffer.getNumSamples();

    for (int i = 0; i < numSamples && interimPos < length; ++i)
    {
        const float value = src[interimPos] * interimGain;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample (ch, i, value);

        ++interimPos;
    }

    if (interimPos >= length)
        interimPos = -1;   // one-shot finished -> idle
}

} // namespace rollforge
