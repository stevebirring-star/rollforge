#pragma once

// RollForge — Step: one step of a sequencer lane (pure data).
//
// MODEL LAYER RULE: no JUCE includes at all — plain POD so it copies cheaply into
// the pattern snapshot the audio thread reads (see engine/TripleBuffer.h).

namespace rollforge
{

struct Step
{
    bool  on          = false;
    float velocity    = 0.8f;   // 0..1
    float microShift  = 0.0f;   // -0.5..+0.5 of a step (Phase 2: only forward/>=0 applied yet)
    int   ratchets    = 1;      // subdivisions within the step, 1..8
    float ratchetRamp = 0.0f;   // -1..+1 velocity ramp across the ratchets
    int   probability = 100;    // percent chance the step fires (100/75/50/25)
    int   sampleLock  = -1;     // -1 = pad's default sample; else alternate index
};

} // namespace rollforge
