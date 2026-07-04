#include "engine/MasterBus.h"

namespace rollforge
{

void MasterBus::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    punch.prepare (sampleRate);
    drive.prepare (sampleRate);
    crush.prepare (sampleRate);
    space.prepare (sampleRate);
    limiter.prepare (sampleRate);
}

void MasterBus::reset() noexcept
{
    punch.reset();
    drive.reset();
    crush.reset();
    space.reset();
    limiter.reset();
}

void MasterBus::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Macro chain (each 0 = bypass): Punch -> Drive -> Crush -> Space (send).
    punch.process (buffer);
    drive.process (buffer);
    crush.process (buffer);
    space.process (buffer);
    // ...then the always-on limiter has the last word.
    limiter.process (buffer);
}

} // namespace rollforge
