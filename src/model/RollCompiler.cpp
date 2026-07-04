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

int compileRollInto (const RollRegion& region, RollEvent* out, int maxEvents) noexcept
{
    if (out == nullptr || maxEvents <= 0)
        return 0;

    const double totalSteps = region.lengthSteps;
    if (totalSteps <= 0.0)
        return 0;

    // Integrate the rate over the span in fine step increments (< ~1 sample at
    // typical tempos), emitting a hit each time the accumulated hit-count crosses
    // an integer.
    const double dt = 1.0 / 8192.0;
    double phase   = 0.0;
    int    nextHit = 0;
    int    count   = 0;

    for (double s = 0.0; s < totalSteps; s += dt)
    {
        const float u = (float) (s / totalSteps);   // 0..1 position along the roll

        while (phase >= (double) nextHit && count < maxEvents)
        {
            out[count].stepOffset     = (float) s;
            out[count].velocity       = clamp01 (curveValue (region.volume, u));
            out[count].pitchSemitones = curveValue (region.pitch, u);
            ++count;
            ++nextHit;
        }
        if (count >= maxEvents)
            break;

        const double rate = (double) curveValue (region.speed, u);   // hits per step
        if (rate > 0.0)
            phase += rate * dt;
    }

    return count;
}

std::vector<RollEvent> compileRoll (const RollRegion& region)
{
    RollEvent buf[maxRollEvents];
    const int n = compileRollInto (region, buf, maxRollEvents);
    return std::vector<RollEvent> (buf, buf + n);
}

CompiledRoll compile (const RollRegion& region)
{
    CompiledRoll cr;
    cr.targetPad = region.targetPad;
    cr.startStep = (float) region.startStep;
    cr.count     = compileRollInto (region, cr.events.data(), maxRollEvents);
    return cr;
}

} // namespace RollCompiler
} // namespace rollforge
