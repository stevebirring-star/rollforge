#pragma once

// RollForge — Slicer: chop a loop into per-pad regions at its onsets.
//
// Pure (no JUCE), so it is headless-testable and runs off the audio thread, in the
// same spirit as library/FeatureExtractor. It shares that file's windowed-RMS
// envelope but peak-picks an ENERGY-FLUX function against an adaptive (median)
// threshold, which finds the ghost notes a fixed "30% of peak" gate misses — the
// difference between chopping a breakbeat usefully and chopping it into 4 pieces.
//
// The output is a list of [startFraction, endFraction) pairs covering the whole
// sample with no gaps or overlaps. Those map DIRECTLY onto the per-pad trim region
// added in Credibility #4 (see engine/VoiceParameters.h), so slicing needs no new
// engine machinery: install one shared SampleBuffer into N pads, give each pad the
// n-th fraction pair, done.

#include <vector>

namespace rollforge
{

/** A slice of a source sample, as fractions of its total length. */
struct Slice
{
    float startFraction = 0.0f;   // [0..1)
    float endFraction   = 1.0f;   // (startFraction..1]
};

namespace Slicer
{
    /** Tuning for onset detection. Defaults suit percussive loops at 44.1 kHz.

        An onset is where the envelope jumps clear of a PEAK FOLLOWER — a running peak
        that decays exponentially, exactly like a compressor's release. That gate is
        what makes this work on real drums.

        A plain "the envelope rose sharply" test does not. A 55 Hz kick has an 18 ms
        period, so a short RMS window sees the waveform oscillate instead of averaging
        it, and the envelope ripples up and down through the whole decay. Any
        scale-invariant rise test then fires on every ripple crest and shatters one kick
        into a dozen onsets. Broadband noise never shows this, so it is easy to write
        passing tests and still be wrong; SlicerTests covers the sine case for that
        reason. The follower stays above a decaying tail, so ripple cannot retrigger,
        while a genuinely new hit clears it. The trailing RMS window (longer than the
        hop) further averages low-frequency ripple without blurring onset timing. */
    struct Options
    {
        int   windowSamples       = 128;   // envelope hop (~2.9 ms @ 44.1 kHz) = onset resolution
        int   envWindowSamples    = 512;   // trailing RMS window (~11.6 ms); >= the hop
        float riseFactor          = 1.4f;  // the envelope must beat the decaying follower by this
        float followerReleaseMs   = 60.0f; // how fast the follower forgets the last hit
        float noiseGateFraction   = 0.02f; // ignore windows quieter than this fraction of the peak
        float minOnsetSpacingMs   = 25.0f; // reject double-triggers closer than this
        float backtrackMs         = 6.0f;  // rewind an onset to the local energy minimum before it

        /** Slicing tiles [0, n) with no gaps, so the first slice has to start at sample 0 and
            the leading onset is snapped there. Anything that cares WHEN a hit happened -- the
            beatbox capture does -- must turn this off, or a recording whose first hit lands
            half a second in reports it on the downbeat. */
        bool  snapFirstToZero     = true;
    };

    /** Onset positions, in SAMPLE indices, ascending. Always starts with 0 when the
        audio is non-empty. Safe on null / empty input (returns empty). */
    std::vector<int> detectOnsets (const float* samples, int numSamples, double sampleRate,
                                   const Options& options = {});

    /** Slices `numSamples` of mono audio into at most `maxSlices` contiguous regions.

        Slices are cut at detected onsets. If fewer than two onsets are found (a
        one-shot, or silence), the sample is divided EVENLY into `maxSlices` regions
        instead — so "slice to 16 pads" always fills 16 pads and the UI never has to
        special-case a dud analysis. If more than `maxSlices` onsets are found, the
        STRONGEST `maxSlices` are kept (by flux) and re-sorted by time, keeping the
        coverage contiguous.

        The returned slices tile [0, 1]: slice[0].startFraction == 0, the last
        endFraction == 1, and slice[i].endFraction == slice[i+1].startFraction. */
    std::vector<Slice> sliceToFractions (const float* samples, int numSamples, double sampleRate,
                                         int maxSlices, const Options& options = {});

    /** Which sequencer step each slice should be placed on to replay the loop in
        order across `stepsPerLoop` steps. Result[i] is the step for slice i, in
        [0, stepsPerLoop). Monotonic but NOT necessarily unique: two slices closer
        together than one step collapse onto the same step, and the caller decides
        what to do (the UI keeps the first). Empty in / empty out. */
    std::vector<int> slicesToSteps (const std::vector<Slice>& slices, int stepsPerLoop);
}

} // namespace rollforge
