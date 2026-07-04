#include "library/FeatureExtractor.h"

#include <cmath>
#include <vector>

namespace rollforge
{
namespace FeatureExtractor
{

AudioFeatures analyse (const float* x, int n, double sampleRate)
{
    AudioFeatures f;
    if (x == nullptr || n <= 0 || sampleRate <= 0.0)
        return f;

    f.durationSeconds = (float) ((double) n / sampleRate);

    // RMS + peak.
    double sumSq = 0.0;
    float  peak  = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::fabs (x[i]);
        if (a > peak) peak = a;
        sumSq += (double) x[i] * (double) x[i];
    }
    f.rms = (float) std::sqrt (sumSq / (double) n);

    // Zero-crossing rate (brightness proxy).
    int zc = 0;
    for (int i = 1; i < n; ++i)
        if ((x[i - 1] < 0.0f) != (x[i] < 0.0f))
            ++zc;
    f.zcr = (float) zc / (float) n;

    // Decay: fraction of samples above 10% of the peak (short = percussive).
    if (peak > 0.0f)
    {
        const float th = 0.1f * peak;
        int above = 0;
        for (int i = 0; i < n; ++i)
            if (std::fabs (x[i]) > th) ++above;
        f.decay = (float) above / (float) n;
    }

    // Onsets: rising edges of a windowed RMS envelope.
    const int win    = 128;
    const int numWin = (n + win - 1) / win;
    std::vector<float> env ((size_t) numWin, 0.0f);
    float maxEnv = 0.0f;
    for (int w = 0; w < numWin; ++w)
    {
        double s = 0.0;
        int    c = 0;
        for (int i = w * win; i < (w + 1) * win && i < n; ++i)
        {
            s += (double) x[i] * (double) x[i];
            ++c;
        }
        const float e = c > 0 ? (float) std::sqrt (s / (double) c) : 0.0f;
        env[(size_t) w] = e;
        if (e > maxEnv) maxEnv = e;
    }
    if (maxEnv > 0.0f)
    {
        const float onTh = 0.3f * maxEnv;
        bool wasBelow = true;
        for (int w = 0; w < numWin; ++w)
        {
            const bool below = env[(size_t) w] < onTh;
            if (wasBelow && ! below) ++f.onsetCount;
            wasBelow = below;
        }
    }

    return f;
}

} // namespace FeatureExtractor
} // namespace rollforge
