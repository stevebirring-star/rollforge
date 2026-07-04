#pragma once

// RollForge — FeatureExtractor: cheap time-domain audio features used to
// auto-categorise samples (Phase 5). Pure (no JUCE), so it is headless-testable
// and runs off the audio thread (the Scanner calls it in a thread pool).
//
// Deliberately FFT-free for now: ZCR is a solid brightness proxy and the decay /
// onset measures separate kicks/toms from hats/claps well. A true spectral
// centroid (FFT) can be added in the Scanner commit if categorisation needs it.

namespace rollforge
{

struct AudioFeatures
{
    float rms             = 0.0f;   // overall level
    float zcr             = 0.0f;   // zero-crossings per sample [0..1] — brightness proxy
    float durationSeconds = 0.0f;
    float decay           = 0.0f;   // fraction of samples above 10% of peak [0..1]; low = percussive
    int   onsetCount      = 0;      // number of amplitude onsets (claps/rolls > 1)
};

namespace FeatureExtractor
{
    /** Analyses `numSamples` of mono audio. Safe on null / empty input. */
    AudioFeatures analyse (const float* samples, int numSamples, double sampleRate);
}

} // namespace rollforge
