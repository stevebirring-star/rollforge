#include "engine/fx/MasterEq.h"

#include <cmath>

namespace rollforge
{

namespace
{
    constexpr double kPi    = 3.14159265358979323846;
    constexpr double lowFc  = 100.0;    // low-shelf corner
    constexpr double midFc  = 1000.0;   // mid peak centre
    constexpr double midQ   = 0.8;      // gentle, wide bell
    constexpr double highFc = 6000.0;   // high-shelf corner
}

void MasterEq::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    // Impossible sentinel gains so the first engaged block always recomputes coeffs.
    lastLow = lastMid = lastHigh = 1.0e9f;
    reset();
}

void MasterEq::reset() noexcept
{
    low.resetState();
    mid.resetState();
    high.resetState();
}

// RBJ Audio-EQ-Cookbook biquads (a0 normalised out). Shelves use slope S = 1.
void MasterEq::setLowShelf (Biquad& bq, double fc, double gainDb) noexcept
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

void MasterEq::setHighShelf (Biquad& bq, double fc, double gainDb) noexcept
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

void MasterEq::setPeak (Biquad& bq, double fc, double q, double gainDb) noexcept
{
    const double A   = std::pow (10.0, gainDb / 40.0);
    const double w0  = 2.0 * kPi * fc / sampleRate;
    const double cw  = std::cos (w0), sw = std::sin (w0);
    const double alp = sw / (2.0 * q);

    const double b0 = 1 + alp * A;
    const double b1 = -2 * cw;
    const double b2 = 1 - alp * A;
    const double a0 = 1 + alp / A;
    const double a1 = -2 * cw;
    const double a2 = 1 - alp / A;

    bq.b0 = (float) (b0 / a0); bq.b1 = (float) (b1 / a0); bq.b2 = (float) (b2 / a0);
    bq.a1 = (float) (a1 / a0); bq.a2 = (float) (a2 / a0);
}

void MasterEq::process (juce::AudioBuffer<float>& buffer) noexcept
{
    float lo = juce::jlimit (-24.0f, 24.0f, lowDb.load  (std::memory_order_acquire));
    float md = juce::jlimit (-24.0f, 24.0f, midDb.load  (std::memory_order_acquire));
    float hi = juce::jlimit (-24.0f, 24.0f, highDb.load (std::memory_order_acquire));

    // All bands flat -> clean pass-through (exact bypass).
    if (std::abs (lo) < 1.0e-4f && std::abs (md) < 1.0e-4f && std::abs (hi) < 1.0e-4f)
        return;

    if (lo != lastLow)  { setLowShelf  (low,  lowFc,  lo);       lastLow  = lo; }
    if (md != lastMid)  { setPeak      (mid,  midFc,  midQ, md);  lastMid  = md; }
    if (hi != lastHigh) { setHighShelf (high, highFc, hi);       lastHigh = hi; }

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;
    const int useCh = numCh < maxCh ? numCh : maxCh;

    for (int c = 0; c < useCh; ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float s = d[i];
            s = low.processSample  (c, s);
            s = mid.processSample  (c, s);
            s = high.processSample (c, s);
            d[i] = s;
        }
    }
}

} // namespace rollforge
