// RollForge — master-bus / limiter smoke tests (Phase 4, commit 1).
//
// Headless: the limiter caps loud + extreme input to its ceiling with no NaN/inf,
// passes quiet signal ~unchanged, and the MasterBus delegates to it.

#include "engine/MasterBus.h"
#include "engine/fx/Drive.h"
#include "engine/fx/MasterLimiter.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    float maxMag (const juce::AudioBuffer<float>& b)
    {
        float m = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
            m = juce::jmax (m, b.getMagnitude (c, 0, b.getNumSamples()));
        return m;
    }

    bool anyNaN (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            const float* p = b.getReadPointer (c);
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (std::isnan (p[i]) || std::isinf (p[i]))
                    return true;
        }
        return false;
    }

    void fill (juce::AudioBuffer<float>& b, float v)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, v);
    }
}

class FxSmokeTest final : public juce::UnitTest
{
public:
    FxSmokeTest() : juce::UnitTest ("RollForge FxSmoke", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("limiter caps a loud signal to the ceiling");
        {
            MasterLimiter lim;
            lim.prepare (sr);
            juce::AudioBuffer<float> b (2, 512);
            fill (b, 2.0f);
            lim.process (b);
            expect (! anyNaN (b));
            expect (maxMag (b) <= lim.getCeiling() + 1.0e-4f);
        }

        beginTest ("limiter passes a quiet signal ~unchanged");
        {
            MasterLimiter lim;
            lim.prepare (sr);
            juce::AudioBuffer<float> b (1, 256);
            for (int i = 0; i < 256; ++i)
                b.setSample (0, i, 0.1f * std::sin ((float) i * 0.2f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            lim.process (b);

            expect (! anyNaN (b));
            float maxDiff = 0.0f;
            for (int i = 0; i < 256; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
            expect (maxDiff < 0.01f);
        }

        beginTest ("limiter survives extreme input without NaN/clip");
        {
            MasterLimiter lim;
            lim.prepare (sr);
            juce::AudioBuffer<float> b (2, 512);
            fill (b, 100.0f);
            lim.process (b);
            expect (! anyNaN (b));
            expect (maxMag (b) <= lim.getCeiling() + 1.0e-4f);
        }

        beginTest ("MasterBus caps output (delegates to the limiter)");
        {
            MasterBus bus;
            bus.prepare (sr, 512);
            juce::AudioBuffer<float> b (2, 512);
            fill (b, 3.0f);
            bus.process (b);
            expect (! anyNaN (b));
            expect (maxMag (b) <= bus.getLimiter().getCeiling() + 1.0e-4f);
        }

        beginTest ("Drive at 0 is a bypass");
        {
            Drive drive;
            drive.prepare (sr);
            drive.setAmount (0.0f);
            juce::AudioBuffer<float> b (1, 256);
            for (int i = 0; i < 256; ++i)
                b.setSample (0, i, 0.3f * std::sin ((float) i * 0.2f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            drive.process (b);

            float maxDiff = 0.0f;
            for (int i = 0; i < 256; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("Drive at full shapes the signal without NaN/blowup");
        {
            Drive drive;
            drive.prepare (sr);
            drive.setAmount (1.0f);
            juce::AudioBuffer<float> b (2, 512);
            fill (b, 0.5f);

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            drive.process (b);

            expect (! anyNaN (b));
            expect (maxMag (b) < 4.0f);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff > 0.001f);
        }
    }
};

static FxSmokeTest fxSmokeTest;

} // namespace rollforge::tests
