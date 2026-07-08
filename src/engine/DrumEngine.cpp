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
    for (auto& m : padMeter)
        m.store (0.0f, std::memory_order_relaxed);
}

void DrumEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    drainCommands();
    renderInto (buffer, 0, buffer.getNumSamples());
    publishPadMeters();   // once per block (renderInto no longer self-publishes)
}

void DrumEngine::drainCommands() noexcept
{
    // Drain both producer queues (message thread, then MIDI) from the block's top.
    commands.drain     ([this] (const EngineCommand& c) { handleCommand (c); });
    midiCommands.drain ([this] (const EngineCommand& c) { handleCommand (c); });
}

void DrumEngine::renderInto (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    pool.renderAdditive (buffer, startSample, numSamples);
}

void DrumEngine::publishPadMeters() noexcept
{
    float levels[maxMeterPads] = { 0.0f };
    pool.addPadLevels (levels, juce::jmin ((int) pads.size(), maxMeterPads));
    for (int p = 0; p < maxMeterPads; ++p)
        padMeter[(size_t) p].store (levels[p], std::memory_order_relaxed);
}

void DrumEngine::setPadMuted (int padIndex, bool shouldBeMuted) noexcept
{
    if (padIndex >= 0 && padIndex < maxMeterPads)
        padMuted[(size_t) padIndex].store (shouldBeMuted, std::memory_order_relaxed);
}

void DrumEngine::setPadSoloed (int padIndex, bool shouldBeSoloed) noexcept
{
    if (padIndex < 0 || padIndex >= maxMeterPads)
        return;
    // exchange keeps soloCount exact even on redundant sets. Only the message thread
    // calls this, so the load/store of soloCount need no stronger ordering.
    const bool was = padSoloed[(size_t) padIndex].exchange (shouldBeSoloed, std::memory_order_relaxed);
    if (was != shouldBeSoloed)
        soloCount.fetch_add (shouldBeSoloed ? 1 : -1, std::memory_order_relaxed);
}

bool DrumEngine::isPadMuted (int padIndex) const noexcept
{
    return padIndex >= 0 && padIndex < maxMeterPads
        && padMuted[(size_t) padIndex].load (std::memory_order_relaxed);
}

bool DrumEngine::isPadSoloed (int padIndex) const noexcept
{
    return padIndex >= 0 && padIndex < maxMeterPads
        && padSoloed[(size_t) padIndex].load (std::memory_order_relaxed);
}

bool DrumEngine::isPadAudible (int padIndex) const noexcept
{
    if (padIndex < 0 || padIndex >= maxMeterPads)
        return true;   // out-of-range: never gate (matches the fallback-blip trigger path)
    if (soloCount.load (std::memory_order_relaxed) > 0)
        return padSoloed[(size_t) padIndex].load (std::memory_order_relaxed);
    return ! padMuted[(size_t) padIndex].load (std::memory_order_relaxed);
}

void DrumEngine::triggerPadNow (int padIndex, float velocity, float pitchOffsetSemitones) noexcept
{
    if (padIndex >= 0 && padIndex < (int) pads.size() && pads[(size_t) padIndex].sample != nullptr)
    {
        const auto& slot = pads[(size_t) padIndex];
        VoiceParameters params = slot.params;
        params.pitchSemitones += pitchOffsetSemitones;
        pool.trigger (slot.sample, params, velocity, slot.chokeGroup, padIndex);
    }
    else
    {
        // No sample configured yet -> fallback blip (keeps the app audible).
        VoiceParameters params;
        params.pitchSemitones = pitchOffsetSemitones;
        pool.trigger (interimSound, params, velocity, 0, padIndex);
    }
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
            triggerPadNow (command.padIndex, command.velocity);
            break;

        case CommandType::none:
        default:
            break;
    }
}

} // namespace rollforge
