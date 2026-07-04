#include "engine/MasterBus.h"

namespace rollforge
{

void MasterBus::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    drive.prepare (sampleRate);
    crush.prepare (sampleRate);
    limiter.prepare (sampleRate);
}

void MasterBus::reset() noexcept
{
    drive.reset();
    crush.reset();
    limiter.reset();
}

void MasterBus::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Macro chain (each 0 = bypass): Drive -> Crush now; Punch/Space land next.
    drive.process (buffer);
    crush.process (buffer);
    // ...then the always-on limiter has the last word.
    limiter.process (buffer);
}

} // namespace rollforge
