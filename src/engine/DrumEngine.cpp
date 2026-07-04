#include "engine/DrumEngine.h"

#include <cmath>

namespace rollforge
{

namespace
{
    // A short synthesised "blip" — the fallback sound for a pad with no sample.
    // ~120 ms, 880 Hz, quadratic fade-out.
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

DrumEngine::DrumEngine (int commandCapacity, int numVoices, int numPads)
    : commands (commandCapacity),
      midiCommands (commandCapacity),
      pool (numVoices),
      pads ((size_t) juce::jmax (1, numPads))
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

bool DrumEngine::pushSetPad (int padIndex,
                             SampleBuffer::Ptr sample,
                             const VoiceParameters& params,
                             int chokeGroup) noexcept
{
    // The command carries a raw pointer; the caller keeps `sample` alive.
    return commands.push (EngineCommand::makeSetPad (padIndex, sample.get(), params, chokeGroup));
}

bool DrumEngine::pushMidiTrigger (int padIndex, float velocity) noexcept
{
    // Serialise MIDI producers (multiple device threads) so the queue stays a
    // valid single-producer FIFO; the audio-thread consumer never locks.
    const juce::SpinLock::ScopedLockType sl (midiProducerLock);
    return midiCommands.push (EngineCommand::makeTrigger (padIndex, velocity));
}

void DrumEngine::prepare (double newSampleRate, int /*maxBlockSize*/)
{
    sampleRate   = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    interimSound = makeInterimBlip (sampleRate);
    pool.prepare (sampleRate);
}

void DrumEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Drain both producer queues (message thread, then MIDI) from the block's top.
    commands.drain     ([this] (const EngineCommand& c) { handleCommand (c); });
    midiCommands.drain ([this] (const EngineCommand& c) { handleCommand (c); });

    pool.renderAdditive (buffer, 0, buffer.getNumSamples());
}

void DrumEngine::handleCommand (const EngineCommand& command) noexcept
{
    const int padIndex = command.padIndex;
    const bool inRange = padIndex >= 0 && padIndex < (int) pads.size();

    switch (command.type)
    {
        case CommandType::setPad:
            if (inRange)
            {
                // Adopt the raw pointer into a Ptr (increfs). Decref of any
                // previous sample is null/held-elsewhere at install time; a live
                // replacement retires the old buffer on the producer side first.
                pads[(size_t) padIndex].sample     = command.sample;
                pads[(size_t) padIndex].params     = command.params;
                pads[(size_t) padIndex].chokeGroup = command.chokeGroup;
            }
            break;

        case CommandType::triggerPad:
            if (inRange && pads[(size_t) padIndex].sample != nullptr)
            {
                const auto& slot = pads[(size_t) padIndex];
                pool.trigger (slot.sample, slot.params, command.velocity, slot.chokeGroup);
            }
            else
            {
                // No sample configured yet -> fallback blip (keeps the app audible).
                pool.trigger (interimSound, VoiceParameters {}, command.velocity, 0);
            }
            break;

        case CommandType::none:
        default:
            break;
    }
}

} // namespace rollforge
