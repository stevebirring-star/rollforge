#pragma once

// RollForge — WavExporter: renders a Pattern to WAV — a full mix and/or per-pad
// stems — via the OfflineRenderer. Lives in the engine layer because it drives the
// renderer (a stem = the same pattern with only one pad's lanes/rolls active).
// With master FX off, the stems sum back to the mix (linear). Off-audio-thread.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/DrumEngine.h"
#include "engine/OfflineRenderer.h"
#include "model/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace rollforge
{
namespace WavExporter
{
    /** Writes a stereo buffer to a 24-bit WAV. */
    bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& file);

    /** Renders the full mix and writes it to `file`. */
    bool exportMix (DrumEngine& engine, const Pattern& pattern, const juce::File& file,
                    const OfflineRenderer::Options& opts);

    /** Renders one pad's stem (only its lanes/rolls) into `out`; returns samples. */
    int renderStem (DrumEngine& engine, const Pattern& pattern, int padIndex,
                    juce::AudioBuffer<float>& out, const OfflineRenderer::Options& opts);

    /** Writes `pad_NN.wav` into `folder` for each pad that has content. Returns the
        number of stems written. */
    int exportStems (DrumEngine& engine, const Pattern& pattern, const juce::File& folder,
                     const OfflineRenderer::Options& opts);
}
} // namespace rollforge
