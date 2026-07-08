#pragma once

// RollForge — VoiceParameters: plain per-note playback parameters.
//
// Pure POD (no JUCE), so it can travel inside a trivially-copyable EngineCommand
// across the lock-free audio FIFO, and is shared by Voice and DrumEngine. Kept
// free of the model layer; DrumEngine/KitInstaller map a model Pad onto this.

namespace rollforge
{

struct VoiceParameters
{
    float gain           = 1.0f;   // linear (1 = unity)
    float pan            = 0.0f;   // -1 = left .. +1 = right
    float pitchSemitones = 0.0f;   // -12 .. +12
    float attackMs       = 0.0f;   // >= 0
    float releaseMs      = 0.0f;   // >= 0
    bool  reverse        = false;
    float startFraction  = 0.0f;   // trim: play from this fraction of the sample [0..1)
    float endFraction    = 1.0f;   // trim: ...to this fraction (kept > startFraction)
};

} // namespace rollforge
