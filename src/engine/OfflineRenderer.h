#pragma once

// RollForge — OfflineRenderer: renders a Pattern to an audio buffer faster than
// realtime, with no audio device. Reuses the exact playback path — a Sequencer +
// the caller's DrumEngine (with its kit) + a MasterBus — so the render matches
// what you hear. Used by the WAV/stem exporter. Off-audio-thread.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/DrumEngine.h"
#include "model/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

namespace OfflineRenderer
{
    struct Options
    {
        double sampleRate    = 44100.0;
        int    blockSize     = 512;
        int    bars          = 1;
        double tailSeconds   = 1.0;    // extra time for decays after the last step
        bool   applyMasterFx = true;

        // The whole master strip, so a render matches what the strip is doing. Every
        // field must default to the bus's own neutral value, or a caller that forgets
        // one silently exports something it isn't hearing.
        float  punch = 0.0f, space = 0.0f, crush = 0.0f, drive = 0.0f;   // macros, 0 = bypass
        float  lowEq = 0.0f, midEq = 0.0f, highEq = 0.0f;                // dB, 0 = flat
        float  comp  = 0.0f;                                             // glue, 0 = bypass
    };

    /** Renders `opts.bars` bars of `pattern` (plus a decay tail) into `out`
        (resized to stereo). Returns the number of samples rendered. */
    int render (DrumEngine& engine, const Pattern& pattern, juce::AudioBuffer<float>& out, const Options& opts);
}

} // namespace rollforge
