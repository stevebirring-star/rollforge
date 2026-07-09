#include "library/BeatboxDetector.h"

#include "library/Categoriser.h"
#include "library/FeatureExtractor.h"

#include <algorithm>
#include <cmath>

namespace rollforge
{
namespace BeatboxDetector
{

namespace
{
    float peakOf (const float* x, int from, int to) noexcept
    {
        float peak = 0.0f;
        for (int i = from; i < to; ++i)
            peak = std::max (peak, std::abs (x[i]));
        return peak;
    }

    /** Eight sample categories collapsed onto the three sounds a mouth actually makes. */
    int padForCategory (SoundCategory category, const Options& options) noexcept
    {
        switch (category)
        {
            case SoundCategory::Kick:
            case SoundCategory::Tom:        return options.kickPad;   // anything low and boomy

            case SoundCategory::HatClosed:
            case SoundCategory::HatOpen:    return options.hatPad;    // anything bright and thin

            case SoundCategory::Snare:
            case SoundCategory::Clap:
            case SoundCategory::Perc:
            case SoundCategory::Fx:
            case SoundCategory::Unknown:
            default:                        return options.snarePad;  // the noisy middle
        }
    }
}

std::vector<Capture::Hit> detect (const float* mono, int numSamples, double sampleRate,
                                  const Options& options)
{
    std::vector<Capture::Hit> hits;

    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return hits;

    const float loudest = peakOf (mono, 0, numSamples);
    if (loudest <= 1.0e-6f)
        return hits;   // silence: no hits, rather than sixteen phantom ones

    const auto onsets = Slicer::detectOnsets (mono, numSamples, sampleRate, options.onsets);
    if (onsets.empty())
        return hits;

    const int judgeSamples = std::max (64, (int) std::llround (options.judgeSeconds * sampleRate));

    hits.reserve (onsets.size());

    for (std::size_t i = 0; i < onsets.size(); ++i)
    {
        const int start = std::clamp (onsets[i], 0, numSamples - 1);

        // Judge a hit by its own window: up to judgeSeconds, but never into the next hit.
        const int nextOnset = (i + 1 < onsets.size()) ? onsets[i + 1] : numSamples;
        const int end = std::min ({ numSamples, start + judgeSamples, std::max (start + 1, nextOnset) });

        const float windowPeak = peakOf (mono, start, end);

        // A breath before the take, or the silence a recording opens with, is not a drum.
        if (windowPeak < options.silenceFraction * loudest)
            continue;

        // Trim to where this hit actually ended. Features measured over trailing silence are
        // features of the silence: a short bright "ts" padded out to 120 ms reads as a snare.
        int judged = end;
        const float tail = options.tailFraction * windowPeak;
        while (judged > start + 1 && std::abs (mono[judged - 1]) < tail)
            --judged;

        const AudioFeatures features = FeatureExtractor::analyse (mono + start, judged - start, sampleRate);

        Capture::Hit hit;
        hit.seconds  = (double) start / sampleRate;
        hit.pad      = padForCategory (Categoriser::fromFeatures (features), options);
        hit.velocity = std::clamp (windowPeak / loudest, 0.0f, 1.0f);
        hits.push_back (hit);
    }

    return hits;
}

} // namespace BeatboxDetector
} // namespace rollforge
