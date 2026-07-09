#include "library/Slicer.h"

#include <algorithm>
#include <cmath>

namespace rollforge
{
namespace Slicer
{

namespace
{
    struct Onset
    {
        int   sample   = 0;
        float strength = 0.0f;   // log-energy rise that produced it
    };

    /** Trailing-window RMS envelope, one value per `hop` samples. The window may be
        longer than the hop (it ends at the hop boundary), which averages away
        low-frequency ripple without delaying the onset: a transient entering the
        window raises the envelope on the very next hop. O(n) via a prefix sum. */
    std::vector<float> rmsEnvelope (const float* x, int n, int hop, int window)
    {
        window = std::max (window, hop);
        const int numWin = (n + hop - 1) / hop;

        std::vector<double> sumSq ((std::size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i)
            sumSq[(std::size_t) i + 1] = sumSq[(std::size_t) i] + (double) x[i] * (double) x[i];

        std::vector<float> env ((std::size_t) numWin, 0.0f);
        for (int w = 0; w < numWin; ++w)
        {
            const int end   = std::min (n, (w + 1) * hop);
            const int start = std::max (0, end - window);
            const int count = end - start;
            env[(std::size_t) w] = count > 0
                ? (float) std::sqrt ((sumSq[(std::size_t) end] - sumSq[(std::size_t) start]) / (double) count)
                : 0.0f;
        }
        return env;
    }

    /** The shared detector. Public detectOnsets/sliceToFractions are thin wrappers:
        slicing needs the strengths to thin an over-long onset list, callers don't. */
    std::vector<Onset> detect (const float* x, int n, double sampleRate, const Options& opt)
    {
        std::vector<Onset> onsets;
        if (x == nullptr || n <= 0 || sampleRate <= 0.0 || opt.windowSamples <= 0)
            return onsets;

        const int hop    = opt.windowSamples;
        const auto env   = rmsEnvelope (x, n, hop, opt.envWindowSamples);
        const int numWin = (int) env.size();
        if (numWin <= 0)
            return onsets;

        const float peakEnv = *std::max_element (env.begin(), env.end());
        if (peakEnv <= 0.0f)
            return onsets;   // silence -> caller falls back to an even division

        const int minSpacingWindows = std::max (1, (int) std::ceil (
            (double) opt.minOnsetSpacingMs * sampleRate / 1000.0 / (double) hop));
        const int backtrackWindows = std::max (0, (int) std::floor (
            (double) opt.backtrackMs * sampleRate / 1000.0 / (double) hop));
        const float gate = opt.noiseGateFraction * peakEnv;

        // Peak follower: holds the last peak, then releases exponentially. A window is
        // an onset only when it clears the follower's CURRENT level by riseFactor, so a
        // decaying tail (whose envelope tracks the follower down) can never retrigger.
        const double releaseSamples = std::max (1.0, (double) opt.followerReleaseMs * sampleRate / 1000.0);
        const float  releaseCoeff   = (float) std::exp (-(double) hop / releaseSamples);

        // Backtrack to the quietest window just before the peak, so a slice starts at
        // the foot of the transient rather than partway up its attack.
        auto footOf = [&] (int peakWindow)
        {
            int   bestWindow = peakWindow;
            float bestEnv    = env[(std::size_t) peakWindow];
            for (int b = peakWindow - 1; b >= 0 && b >= peakWindow - backtrackWindows; --b)
            {
                if (env[(std::size_t) b] <= bestEnv)
                {
                    bestEnv    = env[(std::size_t) b];
                    bestWindow = b;
                }
            }
            return bestWindow * hop;
        };

        /** How loud the hit starting at window w actually gets — used to rank onsets
            when there are more of them than pads. */
        auto peakNear = [&] (int w)
        {
            float best = 0.0f;
            for (int k = w; k < std::min (numWin, w + 8); ++k)
                best = std::max (best, env[(std::size_t) k]);
            return best;
        };

        float follower   = 0.0f;                   // 0 -> a hit at sample 0 always triggers
        int   lastWindow = -minSpacingWindows - 1; // so a window-0 onset is never rejected

        for (int w = 0; w < numWin; ++w)
        {
            const float e = env[(std::size_t) w];

            if (e >= gate && e > follower * opt.riseFactor && w - lastWindow >= minSpacingWindows)
            {
                // Backtracking must never reach back past the previous onset, or the
                // slices would stop ascending and the [start, end) tiling would invert.
                const int foot = onsets.empty() ? footOf (w)
                                                : std::max (footOf (w), onsets.back().sample + 1);
                onsets.push_back ({ foot, peakNear (w) });
                lastWindow = w;
            }

            follower = std::max (e, follower * releaseCoeff);
        }

        // The first slice must start at sample 0, so snap the leading onset there rather
        // than stranding the loop's head (and any pickup) in a slice nobody plays.
        if (! onsets.empty())
            onsets.front().sample = 0;

        return onsets;
    }
}

std::vector<int> detectOnsets (const float* x, int n, double sampleRate, const Options& opt)
{
    const auto onsets = detect (x, n, sampleRate, opt);

    std::vector<int> result;
    result.reserve (onsets.size());
    for (const auto& o : onsets)
        result.push_back (o.sample);
    return result;
}

std::vector<Slice> sliceToFractions (const float* x, int n, double sampleRate,
                                     int maxSlices, const Options& opt)
{
    std::vector<Slice> slices;
    if (n <= 0 || maxSlices <= 0)
        return slices;

    auto onsets = detect (x, n, sampleRate, opt);

    if ((int) onsets.size() < 2)
    {
        // One-shot or silence: divide evenly, so "slice to N pads" always fills N pads.
        slices.reserve ((std::size_t) maxSlices);
        for (int i = 0; i < maxSlices; ++i)
            slices.push_back ({ (float) i / (float) maxSlices,
                                (float) (i + 1) / (float) maxSlices });
        return slices;
    }

    if ((int) onsets.size() > maxSlices)
    {
        // Keep the strongest onsets. Sample 0 always stays: it is boundary 0, and the
        // tiling contract fixes slice 0 to start there.
        std::vector<Onset> rest (onsets.begin() + 1, onsets.end());
        std::stable_sort (rest.begin(), rest.end(),
                          [] (const Onset& a, const Onset& b) { return a.strength > b.strength; });
        rest.resize ((std::size_t) (maxSlices - 1));
        std::sort (rest.begin(), rest.end(),
                   [] (const Onset& a, const Onset& b) { return a.sample < b.sample; });

        onsets.resize (1);
        onsets.insert (onsets.end(), rest.begin(), rest.end());
    }

    slices.reserve (onsets.size());
    const float total = (float) n;
    for (std::size_t i = 0; i < onsets.size(); ++i)
    {
        const int endSample = (i + 1 < onsets.size()) ? onsets[i + 1].sample : n;
        slices.push_back ({ (float) onsets[i].sample / total, (float) endSample / total });
    }

    // Guarantee the tiling contract exactly, free of float drift.
    slices.front().startFraction = 0.0f;
    slices.back().endFraction    = 1.0f;
    for (std::size_t i = 0; i + 1 < slices.size(); ++i)
        slices[i].endFraction = slices[i + 1].startFraction;

    return slices;
}

std::vector<int> slicesToSteps (const std::vector<Slice>& slices, int stepsPerLoop)
{
    std::vector<int> steps;
    if (slices.empty() || stepsPerLoop <= 0)
        return steps;

    steps.reserve (slices.size());
    for (const auto& s : slices)
    {
        int step = (int) std::lround ((double) s.startFraction * (double) stepsPerLoop);
        step = std::clamp (step, 0, stepsPerLoop - 1);
        steps.push_back (step);
    }
    return steps;
}

} // namespace Slicer
} // namespace rollforge
