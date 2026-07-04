#include "engine/VoicePool.h"

namespace rollforge
{

// Choke group meaning "no choke" (mirrors model/Pad.h noChokeGroup; kept literal
// so the engine does not depend on the model layer).
static constexpr int kNoChokeGroup = 0;

VoicePool::VoicePool (int requestedVoices)
    : numVoices (juce::jmax (1, requestedVoices))
{
    voices = std::make_unique<Voice[]> ((size_t) numVoices);
    chokeGroups.assign ((size_t) numVoices, kNoChokeGroup);
    ages.assign ((size_t) numVoices, (juce::int64) 0);
}

void VoicePool::prepare (double newSampleRate)
{
    deviceSampleRate = newSampleRate;
    for (int i = 0; i < numVoices; ++i)
    {
        voices[i].prepare (newSampleRate);
        chokeGroups[(size_t) i] = kNoChokeGroup;
        ages[(size_t) i] = 0;
    }
    nextAge = 0;
}

void VoicePool::reset() noexcept
{
    for (int i = 0; i < numVoices; ++i)
        voices[i].stop();
}

void VoicePool::trigger (SampleBuffer::Ptr sample,
                         const Voice::Parameters& params,
                         float velocity,
                         int chokeGroup) noexcept
{
    if (sample == nullptr)
        return;

    // 1. Choke: stop every active voice sharing this (non-zero) group.
    if (chokeGroup != kNoChokeGroup)
        for (int i = 0; i < numVoices; ++i)
            if (voices[i].isActive() && chokeGroups[(size_t) i] == chokeGroup)
                voices[i].stop();

    // 2. Allocate a voice (reuse idle / steal quietest-oldest).
    const int slot = selectVoice();

    // 3. Start it and record its bookkeeping.
    voices[slot].start (sample, params, velocity);
    chokeGroups[(size_t) slot] = chokeGroup;
    ages[(size_t) slot] = nextAge++;
}

int VoicePool::selectVoice() const noexcept
{
    // (1) Prefer any idle voice.
    for (int i = 0; i < numVoices; ++i)
        if (! voices[i].isActive())
            return i;

    // (2) Steal: lowest level, tie-break oldest, tie-break lowest index.
    int best = 0;
    for (int i = 1; i < numVoices; ++i)
    {
        const float levelI    = voices[i].getLevel();
        const float levelBest = voices[best].getLevel();

        if (levelI < levelBest
            || (levelI == levelBest && ages[(size_t) i] < ages[(size_t) best]))
            best = i;
    }
    return best;
}

void VoicePool::renderAdditive (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    for (int i = 0; i < numVoices; ++i)
        voices[i].renderAdditive (buffer, startSample, numSamples);
}

int VoicePool::getNumActive() const noexcept
{
    int n = 0;
    for (int i = 0; i < numVoices; ++i)
        if (voices[i].isActive())
            ++n;
    return n;
}

bool VoicePool::isVoiceActive (int index) const noexcept
{
    return index >= 0 && index < numVoices && voices[index].isActive();
}

float VoicePool::getVoiceLevel (int index) const noexcept
{
    return (index >= 0 && index < numVoices) ? voices[index].getLevel() : 0.0f;
}

juce::int64 VoicePool::getVoiceAge (int index) const noexcept
{
    return (index >= 0 && index < numVoices) ? ages[(size_t) index] : 0;
}

} // namespace rollforge
