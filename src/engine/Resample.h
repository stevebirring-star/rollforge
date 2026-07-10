#pragma once

// RollForge — Resample: turn a rendered loop into a sample that loops.
//
// Bouncing a bar of a drum pattern and truncating it at the bar line cuts its own decay: the
// crash you put on beat one dies mid-ring, and the reverb tail you dialled in never happens.
// Rendering with a tail instead and just keeping it makes a sample LONGER than the loop it
// came from, so retriggering it on the grid smears.
//
// The fix every producer does by hand: render past the end, then fold the overhang back over
// the start. The tail lands where it would have landed on the loop's next pass, which is
// exactly where the ear expects it. What comes out is `loopSamples` long and seamless.
//
// Pure: juce_audio_basics only, no GUI, off the audio thread. Headless-testable.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{
namespace Resample
{
    /** Samples in `bars` bars of 4/4 at `bpm`. Rounded, never below 1. */
    int loopSamples (double sampleRate, double bpm, int bars) noexcept;

    /** Folds everything past `loopSamples` back over the start (wrapping as many times as the
        tail is long), then shortens `buffer` to `loopSamples`.

        A no-op when the buffer is already no longer than the loop. Returns the peak magnitude
        of the result, which the fold can push above what the render's limiter allowed —
        summing a tail onto a downbeat adds energy the limiter never saw. */
    float foldTailIntoLoop (juce::AudioBuffer<float>& buffer, int loopSamples);

    /** Scales `buffer` so its peak is exactly `target`. Does nothing when the buffer is
        silent, or when its peak is already at or below `target`. Returns the gain applied. */
    float limitPeak (juce::AudioBuffer<float>& buffer, float target = 0.99f);
}
} // namespace rollforge
