#pragma once

// RollForge — MasterBus: the master output chain applied after the Sequencer mix.
// Phase 4 grows the four macro effects (Punch / Space / Crush / Drive) here; for
// now it is just the always-on MasterLimiter. RT-safe; 0 effects = clean pass
// through (plus limiting).
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/fx/MasterLimiter.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

class MasterBus
{
public:
    void prepare (double sampleRate, int blockSize) noexcept;
    void reset() noexcept;

    /** Processes the master mix in place: (future) macro FX chain, then the limiter. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    MasterLimiter& getLimiter() noexcept { return limiter; }

private:
    MasterLimiter limiter;
};

} // namespace rollforge
