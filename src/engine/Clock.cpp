#include "engine/Clock.h"

#include <cmath>

namespace rollforge
{

void Clock::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    setTempo (bpm);   // recompute samplesPerStep for the new rate
    reset();
}

void Clock::setTempo (double newBpm) noexcept
{
    // Rebase so the next (not-yet-fired) step keeps its scheduled sample; later
    // steps are respaced at the new tempo. This keeps a mid-play tempo change
    // continuous (no jump of the pending step).
    baseSample = stepSampleOf (nextStepIndex);
    baseStep   = nextStepIndex;

    bpm = newBpm > 0.0 ? newBpm : 120.0;

    // One 1/16 note = a quarter note / 4.
    const double perStep = sampleRate * 60.0 / (bpm * 4.0);
    samplesPerStep = perStep < 1.0 ? 1.0 : perStep;   // guard absurd tempos
}

void Clock::reset() noexcept
{
    sampleCounter = 0;
    nextStepIndex = 0;
    baseStep      = 0;
    baseSample    = 0;
}

std::int64_t Clock::stepSampleOf (std::int64_t index) const noexcept
{
    return baseSample + (std::int64_t) std::llround ((double) (index - baseStep) * samplesPerStep);
}

} // namespace rollforge
