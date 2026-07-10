#include "engine/fx/ToneFilter.h"

#include <cmath>

namespace rollforge
{

namespace
{
    constexpr double kPi     = 3.14159265358979323846;
    constexpr double pivotFc = 700.0;   // where dark and bright trade off

    float clampTone (float t) noexcept { return t < -1.0f ? -1.0f : (t > 1.0f ? 1.0f : t); }
}

// RBJ Audio-EQ-Cookbook shelves (a0 normalised out), slope S = 1. Mirrors MasterEq.
void ToneFilter::setLowShelf (Biquad& bq, double sampleRate, double fc, double gainDb) noexcept
{
    const double A   = std::pow (10.0, gainDb / 40.0);
    const double w0  = 2.0 * kPi * fc / sampleRate;
    const double cw  = std::cos (w0), sw = std::sin (w0);
    const double alp = sw / 2.0 * std::sqrt (2.0);
    const double tsa = 2.0 * std::sqrt (A) * alp;

    const double b0 =    A * ((A + 1) - (A - 1) * cw + tsa);
    const double b1 =  2*A * ((A - 1) - (A + 1) * cw);
    const double b2 =    A * ((A + 1) - (A - 1) * cw - tsa);
    const double a0 =        (A + 1) + (A - 1) * cw + tsa;
    const double a1 =   -2 * ((A - 1) + (A + 1) * cw);
    const double a2 =        (A + 1) + (A - 1) * cw - tsa;

    bq.b0 = (float) (b0 / a0); bq.b1 = (float) (b1 / a0); bq.b2 = (float) (b2 / a0);
    bq.a1 = (float) (a1 / a0); bq.a2 = (float) (a2 / a0);
}

void ToneFilter::setHighShelf (Biquad& bq, double sampleRate, double fc, double gainDb) noexcept
{
    const double A   = std::pow (10.0, gainDb / 40.0);
    const double w0  = 2.0 * kPi * fc / sampleRate;
    const double cw  = std::cos (w0), sw = std::sin (w0);
    const double alp = sw / 2.0 * std::sqrt (2.0);
    const double tsa = 2.0 * std::sqrt (A) * alp;

    const double b0 =    A * ((A + 1) + (A - 1) * cw + tsa);
    const double b1 = -2*A * ((A - 1) + (A + 1) * cw);
    const double b2 =    A * ((A + 1) + (A - 1) * cw - tsa);
    const double a0 =        (A + 1) - (A - 1) * cw + tsa;
    const double a1 =    2 * ((A - 1) - (A + 1) * cw);
    const double a2 =        (A + 1) - (A - 1) * cw - tsa;

    bq.b0 = (float) (b0 / a0); bq.b1 = (float) (b1 / a0); bq.b2 = (float) (b2 / a0);
    bq.a1 = (float) (a1 / a0); bq.a2 = (float) (a2 / a0);
}

void ToneFilter::reset() noexcept
{
    low.resetState();
    high.resetState();
}

void ToneFilter::setTone (double sampleRate, float tone) noexcept
{
    reset();

    const float t = clampTone (tone);
    active = std::abs (t) > 1.0e-4f;
    if (! active)
        return;   // exact bypass: processSample is never called

    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double db = (double) t * (double) maxTiltDb;

    // Tilt: the shelves move in opposite directions about the pivot.
    setLowShelf  (low,  sr, pivotFc, -db);
    setHighShelf (high, sr, pivotFc, +db);
}

} // namespace rollforge
