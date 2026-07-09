#include "model/Capture.h"

#include <algorithm>
#include <cmath>

namespace rollforge
{
namespace Capture
{

int laneForPad (const Pattern& pattern, int pad) noexcept
{
    const int lanes = std::clamp (pattern.numLanes, 0, maxLanes);
    for (int i = 0; i < lanes; ++i)
        if (pattern.lane (i).targetPad == pad)
            return i;
    return -1;
}

bool quantise (const Pattern& pattern, const Hit& hit, double bpm,
               int& laneOut, int& stepOut) noexcept
{
    const int lane = laneForPad (pattern, hit.pad);
    if (lane < 0)
        return false;

    const double tempo = bpm > 0.0 ? bpm : 120.0;
    const Lane&  l     = pattern.lane (lane);
    const int    len   = std::clamp (l.length, 1, maxStepsPerLane);

    // One bar is four beats. A straight lane cuts it into sixteen, a triplet lane into twelve.
    const int    stepsPerBar   = l.triplet ? tripletStepsPerBar : straightStepsPerBar;
    const double secondsPerBar = 4.0 * 60.0 / tempo;
    const double secondsPerStep = secondsPerBar / (double) stepsPerBar;

    // llround, not truncation: a hit 1 ms before the loop point belongs on the downbeat, and
    // the modulo below is what puts it there rather than on the last step of the bar.
    long long step = std::llround (hit.seconds / secondsPerStep);
    step %= (long long) len;
    if (step < 0)
        step += len;

    laneOut = lane;
    stepOut = (int) step;
    return true;
}

Step merge (const Step& existing, float velocity) noexcept
{
    const float v = std::clamp (velocity, 0.0f, 1.0f);

    // Two taps rounded onto one step are one note. The louder one is the one they meant; the
    // quieter is the flam they did not.
    if (existing.on)
    {
        Step out = existing;
        out.velocity = std::max (existing.velocity, v);
        return out;
    }

    // A clean single tap. Whatever ratchets, probability or micro-shift were programmed into
    // this step, the user just played a note over them.
    Step out;
    out.on       = true;
    out.velocity = v;
    return out;
}

Result apply (Pattern& pattern, const std::vector<Hit>& hits, const Options& options)
{
    Result result;

    if (options.replace)
    {
        const int lanes = std::clamp (pattern.numLanes, 0, maxLanes);
        for (int i = 0; i < lanes; ++i)
            for (int s = 0; s < maxStepsPerLane; ++s)
                pattern.lane (i).step (s) = Step {};
    }

    for (const auto& hit : hits)
    {
        if (hit.velocity < options.minVelocity)
        {
            ++result.dropped;
            continue;
        }

        int lane = 0, step = 0;
        if (! quantise (pattern, hit, options.bpm, lane, step))
        {
            ++result.dropped;
            continue;
        }

        Step& target = pattern.lane (lane).step (step);
        const bool wasOn = target.on;

        target = merge (target, hit.velocity);

        if (wasOn) ++result.merged;
        else       ++result.placed;
    }

    return result;
}

} // namespace Capture
} // namespace rollforge
