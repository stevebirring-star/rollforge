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
    return pushSetPadLayers (padIndex, &sample, sample != nullptr ? 1 : 0,
                             LayerMode::roundRobin, params, chokeGroup);
}

bool DrumEngine::pushSetPadLayers (int padIndex,
                                   const SampleBuffer::Ptr* layers,
                                   int numLayers,
                                   LayerMode layerMode,
                                   const VoiceParameters& params,
                                   int chokeGroup) noexcept
{
    // The command carries raw pointers; the caller keeps every layer alive.
    SampleBuffer* raw[maxPadLayers] {};
    int n = 0;
    for (int i = 0; i < numLayers && n < maxPadLayers; ++i)
        if (layers[i] != nullptr)
            raw[n++] = layers[i].get();

    return commands.push (EngineCommand::makeSetPad (padIndex, raw, n, (int) layerMode,
                                                     params, chokeGroup));
}

bool DrumEngine::pushMidiTrigger (int padIndex, float velocity) noexcept
{
    // Serialise MIDI producers (multiple device threads) so the queue stays a
    // valid single-producer FIFO; the audio-thread consumer never locks.
    const juce::SpinLock::ScopedLockType sl (midiProducerLock);
    return midiCommands.push (EngineCommand::makeTrigger (padIndex, velocity));
}

void DrumEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate   = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    interimSound = makeInterimBlip (sampleRate);
    pool.prepare (sampleRate);
    for (auto& m : padMeter)
        m.store (0.0f, std::memory_order_relaxed);

    // Allocation is permitted here (stream start), never in process(). Generous slack:
    // a host must never hand process() a block larger than it prepared us for, but the
    // send write is bounds-checked in renderInto() regardless.
    sendReverb.prepare (sampleRate);
    sendReverb.reset();
    sendBuffer.setSize (1, juce::jmax (maxBlockSize, 8192), false, true, false);
    sendBuffer.clear();
    sendTailSamples = 0;

    // Reset every render, so an offline render (which prepares first) is reproducible
    // and a per-pad stem starts from the same reverb state — and the same round-robin
    // position — as the mix. Without this, exporting twice would give different files.
    for (auto& slot : pads)
        slot.roundRobin = 0;
}

void DrumEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    drainCommands();
    renderInto (buffer, 0, buffer.getNumSamples());
    applySendReturn (buffer, buffer.getNumSamples());
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
    // Voices accumulate their sends at the same block-relative offsets they write dry
    // audio to, so many render segments in one block all land in the right places.
    float* sendOut = nullptr;
    if (startSample >= 0 && numSamples >= 0 && startSample + numSamples <= sendBuffer.getNumSamples())
        sendOut = sendBuffer.getWritePointer (0);

    pool.renderAdditive (buffer, startSample, numSamples, sendOut);
}

void DrumEngine::applySendReturn (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (numSamples <= 0 || numSamples > sendBuffer.getNumSamples())
        return;

    // Keep reverberating for a while after the last send is turned down, or the tail of
    // the hit that fed it would be chopped off mid-decay.
    //
    // The gate is deliberately the pad CONFIGURATION, not whether the send buffer has
    // energy in it this block. A per-pad stem renders one pad's hits through an engine
    // that still has all 16 pads configured, so a configuration-based gate is identical
    // in the mix and in every stem — which is what lets the stems sum back to the mix.
    // Gating on energy would idle the reverb during a quiet stem and not during the mix,
    // and the two would stop agreeing. The cost of running it over silence is ~10 flops
    // a sample, and the ScopedNoDenormals in the callers keeps that true.
    if (anySendActive)
        sendTailSamples = (int) (sampleRate * 3.0);
    else
        sendTailSamples = juce::jmax (0, sendTailSamples - numSamples);

    if (! anySendActive && sendTailSamples == 0)
    {
        sendReverb.reset();                        // silence the (already inaudible) lines
        sendBuffer.clear (0, 0, numSamples);
        return;
    }

    sendReverb.processSend (sendBuffer.getReadPointer (0), buffer, numSamples);
    sendBuffer.clear (0, 0, numSamples);           // the reverb's own delay lines persist
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

int DrumEngine::pickLayer (PadSlot& slot, float velocity, int sampleLock) noexcept
{
    if (slot.numLayers <= 1)
        return 0;

    // An explicit per-step lock always wins, so a pinned alternate stays pinned.
    if (sampleLock >= 0 && sampleLock < slot.numLayers)
        return sampleLock;

    if (slot.layerMode == (int) LayerMode::velocity)
    {
        int index = (int) ((velocity < 0.0f ? 0.0f : velocity) * (float) slot.numLayers);
        return index >= slot.numLayers ? slot.numLayers - 1 : index;
    }

    const int index = slot.roundRobin;
    slot.roundRobin = (slot.roundRobin + 1) % slot.numLayers;
    return index;
}

void DrumEngine::triggerPadNow (int padIndex, float velocity, float pitchOffsetSemitones,
                                int sampleLock) noexcept
{
    if (padIndex >= 0 && padIndex < (int) pads.size() && pads[(size_t) padIndex].numLayers > 0)
    {
        auto& slot = pads[(size_t) padIndex];
        VoiceParameters params = slot.params;
        params.pitchSemitones += pitchOffsetSemitones;
        pool.trigger (slot.layers[(size_t) pickLayer (slot, velocity, sampleLock)],
                      params, velocity, slot.chokeGroup, padIndex);
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
                auto& slot = pads[(size_t) padIndex];
                for (int i = 0; i < maxPadLayers; ++i)
                    slot.layers[(size_t) i] = i < command.numLayers ? command.layers[i] : nullptr;
                slot.numLayers  = command.numLayers;
                slot.layerMode  = command.layerMode;
                slot.params     = command.params;
                slot.chokeGroup = command.chokeGroup;
                // slot.roundRobin is deliberately untouched: re-pushing a pad's params
                // (a knob move) must not restart its round-robin cycle.

                // Gate the send reverb on whether any pad actually uses it. Audio-thread
                // state derived from audio-thread state, so no atomic is needed.
                anySendActive = false;
                for (const auto& p : pads)
                    if (p.params.reverbSend > 0.0f)
                    {
                        anySendActive = true;
                        break;
                    }
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
