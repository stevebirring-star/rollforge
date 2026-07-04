#include "engine/MasterBus.h"

namespace rollforge
{

void MasterBus::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    limiter.prepare (sampleRate);
}

void MasterBus::reset() noexcept
{
    limiter.reset();
}

void MasterBus::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Phase 4: Drive -> Crush -> Punch chain + Space send will go here (each
    // 0 = bypass), then the always-on limiter has the last word.
    limiter.process (buffer);
}

} // namespace rollforge
