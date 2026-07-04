#include "engine/DrumEngine.h"

#include <cmath>

namespace rollforge
{

namespace
{
    // A short synthesised "blip" — the INTERIM trigger sound until StarterKit
    // installs real per-pad samples. ~120 ms, 880 Hz, quadratic fade-out.
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

DrumEngine::DrumEngine (int commandCapacity, int numVoices)
    : commands (commandCapacity),
      pool (numVoices)
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
    pool.prepare (sampleRate);
}

void DrumEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Drain producer commands first, so a trigger applies from the block's top.
    commands.drain ([this] (const EngineCommand& c) { handleCommand (c); });

    pool.renderAdditive (buffer, 0, buffer.getNumSamples());
}

void DrumEngine::handleCommand (const EngineCommand& command) noexcept
{
    switch (command.type)
    {
        case CommandType::triggerPad:
            // Interim: every pad plays the same synth blip with default voice
            // params and no choke group. Real per-pad sample + Voice::Parameters
            // + choke-group mapping arrives with StarterKit (7/9).
            pool.trigger (interimSound, Voice::Parameters {}, command.velocity, 0);
            break;

        case CommandType::none:
        default:
            break;
    }
}

} // namespace rollforge
