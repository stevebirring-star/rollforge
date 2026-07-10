// RollForge — master-bus / limiter smoke tests (Phase 4, commit 1).
//
// Headless: the limiter caps loud + extreme input to its ceiling with no NaN/inf,
// passes quiet signal ~unchanged, and the MasterBus delegates to it.

#include "engine/MasterBus.h"
#include "engine/fx/Compressor.h"
#include "engine/fx/Crush.h"
#include "engine/fx/Drive.h"
#include "engine/fx/MasterEq.h"
#include "engine/fx/MasterLimiter.h"
#include "engine/fx/Punch.h"
#include "engine/fx/Space.h"
#include "engine/fx/ToneFilter.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

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

        beginTest ("a lamp watching the limiter's OUTPUT could never light");
        {
            // The reason the clip lamp was rewired. The limiter hard-clamps every sample to
            // its ceiling, so the loudest thing that can ever leave it is 0.98 -- about
            // -0.18 dBFS. Any clip detector fed from the output is therefore dead on arrival,
            // whatever its threshold, as long as that threshold is above the ceiling.
            MasterLimiter lim;
            lim.prepare (sr);
            expect (lim.getCeiling() < 1.0f, "the ceiling is what makes the output safe");

            juce::AudioBuffer<float> b (2, 512);
            fill (b, 40.0f);          // absurd, on purpose
            lim.process (b);
            expect (maxMag (b) <= lim.getCeiling() + 1.0e-4f,
                    "nothing above the ceiling ever leaves the limiter");
        }

        beginTest ("the limiter reports the peak it was HANDED, which is what a clip lamp needs");
        {
            MasterLimiter lim;
            lim.prepare (sr);

            expectWithinAbsoluteError (lim.readAndResetInputPeak(), 0.0f, 1.0e-6f,
                                       "a fresh limiter has seen nothing");

            juce::AudioBuffer<float> b (2, 512);
            fill (b, 1.7f);
            lim.process (b);

            expectWithinAbsoluteError (lim.readAndResetInputPeak(), 1.7f, 1.0e-4f,
                                       "the input peak must survive the clamp");

            // Reading drains it: a clip shown once must not latch forever.
            expectWithinAbsoluteError (lim.readAndResetInputPeak(), 0.0f, 1.0e-6f,
                                       "the peak was not drained by the read");

            // ...and a quiet block never claims to have gone over.
            juce::AudioBuffer<float> quiet (1, 256);
            for (int i = 0; i < 256; ++i)
                quiet.setSample (0, i, 0.4f * std::sin ((float) i * 0.2f));
            lim.process (quiet);
            expect (lim.readAndResetInputPeak() < 1.0f, "a quiet block reported a clip");
        }

        beginTest ("the loudest block wins, not the last one");
        {
            // The meter reads at 30 Hz; many audio blocks pass between reads, and a transient
            // that peaked in the first of them must still be there when the lamp looks.
            MasterLimiter lim;
            lim.prepare (sr);

            juce::AudioBuffer<float> loud (1, 128);
            fill (loud, 2.5f);
            lim.process (loud);

            for (int i = 0; i < 5; ++i)
            {
                juce::AudioBuffer<float> quiet (1, 128);
                fill (quiet, 0.05f);
                lim.process (quiet);
            }

            expectWithinAbsoluteError (lim.readAndResetInputPeak(), 2.5f, 1.0e-4f,
                                       "quiet blocks erased the transient the lamp exists to show");
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

        beginTest ("Crush at 0 is a bypass");
        {
            Crush crush;
            crush.prepare (sr);
            crush.setAmount (0.0f);
            juce::AudioBuffer<float> b (1, 256);
            for (int i = 0; i < 256; ++i)
                b.setSample (0, i, 0.4f * std::sin ((float) i * 0.15f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            crush.process (b);

            float maxDiff = 0.0f;
            for (int i = 0; i < 256; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("Crush at full quantises the signal without NaN/blowup");
        {
            Crush crush;
            crush.prepare (sr);
            crush.setAmount (1.0f);
            juce::AudioBuffer<float> b (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, 0.5f * std::sin ((float) i * 0.1f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            crush.process (b);

            expect (! anyNaN (b));
            expect (maxMag (b) < 2.0f);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff > 0.001f);
        }

        beginTest ("Punch at 0 is a bypass");
        {
            Punch punch;
            punch.prepare (sr);
            punch.setAmount (0.0f);
            juce::AudioBuffer<float> b (1, 256);
            for (int i = 0; i < 256; ++i)
                b.setSample (0, i, 0.5f * std::sin ((float) i * 0.12f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            punch.process (b);

            float maxDiff = 0.0f;
            for (int i = 0; i < 256; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("Punch at full emphasises without NaN/blowup");
        {
            Punch punch;
            punch.prepare (sr);
            punch.setAmount (1.0f);
            juce::AudioBuffer<float> b (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, 0.5f * std::sin ((float) i * 0.1f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            punch.process (b);

            expect (! anyNaN (b));
            expect (maxMag (b) < 8.0f);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff > 0.001f);
        }

        beginTest ("Space at 0 is a bypass");
        {
            Space space;
            space.prepare (sr);
            space.setAmount (0.0f);
            juce::AudioBuffer<float> b (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, 0.3f * std::sin ((float) i * 0.1f));

            juce::AudioBuffer<float> ref;
            ref.makeCopyOf (b);
            space.process (b);

            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("Space at full adds a reverb tail without NaN/blowup");
        {
            Space space;
            space.prepare (sr);
            space.setAmount (1.0f);
            juce::AudioBuffer<float> b (2, 8192);
            b.clear();
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    b.setSample (c, i, 0.6f * std::sin ((float) i * 0.3f));   // burst, then silence

            space.process (b);

            expect (! anyNaN (b));
            expect (maxMag (b) < 4.0f);

            // A reverb tail appears in the originally-silent region.
            float tail = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 2000; i < 8192; ++i)
                    tail = juce::jmax (tail, std::abs (b.getSample (c, i)));
            expect (tail > 0.0f);
        }

        beginTest ("MasterEq flat (0 dB) is a bypass");
        {
            MasterEq eq; eq.prepare (sr);
            eq.setLowDb (0.0f); eq.setMidDb (0.0f); eq.setHighDb (0.0f);
            juce::AudioBuffer<float> b (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, 0.3f * std::sin ((float) i * 0.1f));

            juce::AudioBuffer<float> ref; ref.makeCopyOf (b);
            eq.process (b);

            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("MasterEq boost changes the signal without NaN/blowup");
        {
            MasterEq eq; eq.prepare (sr);
            eq.setLowDb (6.0f); eq.setHighDb (12.0f);
            juce::AudioBuffer<float> b (2, 2048);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 2048; ++i)
                    b.setSample (c, i, 0.3f * std::sin ((float) i * 0.05f));

            juce::AudioBuffer<float> ref; ref.makeCopyOf (b);
            eq.process (b);

            expect (! anyNaN (b));
            expect (maxMag (b) < 4.0f);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 2048; ++i)
                    maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (c, i) - ref.getSample (c, i)));
            expect (maxDiff > 0.001f);
        }

        beginTest ("Compressor at 0 is a bypass");
        {
            Compressor comp; comp.prepare (sr);
            comp.setAmount (0.0f);
            juce::AudioBuffer<float> b (1, 256);
            for (int i = 0; i < 256; ++i)
                b.setSample (0, i, 0.4f * std::sin ((float) i * 0.15f));

            juce::AudioBuffer<float> ref; ref.makeCopyOf (b);
            comp.process (b);

            float maxDiff = 0.0f;
            for (int i = 0; i < 256; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
            expect (maxDiff == 0.0f);
        }

        beginTest ("Compressor at full attenuates loud more than quiet (glue), no NaN");
        {
            auto steadyGain = [&] (float level)
            {
                Compressor comp; comp.prepare (sr);
                comp.setAmount (1.0f);
                const int n = 8192;
                juce::AudioBuffer<float> b (2, n);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                        b.setSample (c, i, level);
                comp.process (b);
                expect (! anyNaN (b));
                return b.getSample (0, n - 1) / level;   // steady-state output / input
            };

            const float gLoud  = steadyGain (0.5f);    // above the -24 dBFS threshold
            const float gQuiet = steadyGain (0.03f);   // below it
            expect (gLoud > 0.0f && gQuiet > 0.0f);
            expect (gLoud < gQuiet);                    // the loud signal is compressed more
        }

        beginTest ("MasterBus still caps output with EQ + comp engaged");
        {
            MasterBus bus; bus.prepare (sr, 512);
            bus.setLowEqDb (12.0f); bus.setHighEqDb (12.0f); bus.setComp (1.0f);
            juce::AudioBuffer<float> b (2, 512);
            fill (b, 0.8f);
            bus.process (b);
            expect (! anyNaN (b));
            expect (maxMag (b) <= bus.getLimiter().getCeiling() + 1.0e-4f);
        }

        beginTest ("ToneFilter: tone 0 is an exact bypass; the tilt has the right sign");
        {
            // A two-tone probe: energy well below the 700 Hz pivot, and well above it.
            auto probe = [sr] (double freq, int n)
            {
                std::vector<float> x ((std::size_t) n);
                for (int i = 0; i < n; ++i)
                    x[(std::size_t) i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
                return x;
            };
            auto rms = [] (const std::vector<float>& x)
            {
                double s = 0.0;
                for (float v : x) s += (double) v * v;
                return std::sqrt (s / (double) x.size());
            };
            auto run = [&] (float tone, double freq)
            {
                auto x = probe (freq, 8192);
                ToneFilter f;
                f.setTone (sr, tone);
                if (! f.isActive())
                    return rms (x);
                std::vector<float> y = x;
                for (auto& v : y) v = f.processSample (v);
                // Skip the transient: measure the settled second half.
                return rms (std::vector<float> (y.begin() + 4096, y.end()));
            };

            ToneFilter flat;
            flat.setTone (sr, 0.0f);
            expect (! flat.isActive(), "tone 0 must bypass, not run unity coefficients");

            const double lowRef  = run (0.0f, 100.0);
            const double highRef = run (0.0f, 8000.0);

            // Bright: lows cut, highs boosted. Dark: the reverse.
            expect (run (+1.0f, 100.0)  < lowRef  * 0.9, "bright cuts the lows");
            expect (run (+1.0f, 8000.0) > highRef * 1.1, "bright boosts the highs");
            expect (run (-1.0f, 100.0)  > lowRef  * 1.1, "dark boosts the lows");
            expect (run (-1.0f, 8000.0) < highRef * 0.9, "dark cuts the highs");

            // No blow-ups at the extremes.
            for (float t : { -1.0f, -0.5f, 0.5f, 1.0f })
            {
                ToneFilter f; f.setTone (sr, t);
                for (int i = 0; i < 4096; ++i)
                    expect (! std::isnan (f.processSample (i % 2 ? 0.9f : -0.9f)));
            }
        }

        beginTest ("Space::processSend rings on after its input stops, and is linear");
        {
            // Longer than the shortest comb (1116 samples), or the first echo hasn't
            // even emerged by the end of block 1 and "the tail" measures nothing.
            const int n = 2048;
            auto tailAfterImpulse = [&] (float amplitude)
            {
                Space s; s.prepare (sr); s.reset();
                std::vector<float> in ((std::size_t) n, 0.0f);
                in[0] = amplitude;

                juce::AudioBuffer<float> out (2, n);
                out.clear();
                s.processSend (in.data(), out, n);      // block 1: the impulse

                juce::AudioBuffer<float> tail (2, n);
                tail.clear();
                std::vector<float> silence ((std::size_t) n, 0.0f);
                s.processSend (silence.data(), tail, n); // block 2: silence in
                return tail;
            };

            auto tail = tailAfterImpulse (1.0f);
            expect (maxMag (tail) > 0.0f, "the tail keeps decaying after the input stops");
            expect (! anyNaN (tail));

            // Linearity is what keeps per-pad stems summing to the mix (StemNullTests).
            auto one   = tailAfterImpulse (1.0f);
            auto two   = tailAfterImpulse (2.0f);
            float err = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    err = juce::jmax (err, std::abs (2.0f * one.getSample (c, i) - two.getSample (c, i)));
            expect (err < 1.0e-5f, "reverb(2x) == 2*reverb(x)");
        }
    }
};

static FxSmokeTest fxSmokeTest;

} // namespace rollforge::tests
