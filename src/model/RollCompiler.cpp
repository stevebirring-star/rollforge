#include "model/RollCompiler.h"

#include <cmath>

namespace rollforge
{
namespace RollCompiler
{

namespace
{
    float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}

float curveValue (const RollCurve& curve, float t) noexcept
{
    t = clamp01 (t);
    const float shape    = curve.shape < -1.0f ? -1.0f : (curve.shape > 1.0f ? 1.0f : curve.shape);
    const float exponent = std::pow (2.0f, shape * 2.0f);   // shape 0 -> exponent 1 (linear)
    const float eased    = std::pow (t, exponent);
    return curve.start + (curve.end - curve.start) * eased;
}

std::vector<RollEvent> compileRoll (const RollRegion& region, double samplesPerStep)
{
    std::vector<RollEvent> events;

    if (region.lengthSteps <= 0.0 || samplesPerStep <= 0.0)
        return events;

    const int totalSamples = (int) std::llround (region.lengthSteps * samplesPerStep);
    if (totalSamples <= 0)
        return events;

    double phase   = 0.0;   // accumulated hit count (integral of the rate)
    int    nextHit = 0;

    for (int sample = 0; sample < totalSamples; ++sample)
    {
        const float u = (float) sample / (float) totalSamples;   // 0..1 position along the roll

        // Emit any hits whose phase boundary has been reached (usually one).
        while (phase >= (double) nextHit)
        {
            RollEvent e;
            e.sampleOffset   = sample;
            e.velocity       = clamp01 (curveValue (region.volume, u));
            e.pitchSemitones = curveValue (region.pitch, u);
            events.push_back (e);
            ++nextHit;
        }

        const double rateHitsPerStep = (double) curveValue (region.speed, u);
        if (rateHitsPerStep > 0.0)
            phase += rateHitsPerStep / samplesPerStep;
    }

    return events;
}

} // namespace RollCompiler
} // namespace rollforge
