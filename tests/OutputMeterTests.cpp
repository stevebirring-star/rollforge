// RollForge — master VU / peak meter tests.
//
// Headless + pure. The point of a true VU is its 300 ms integration: it must be slow
// enough to ignore a drum transient and read loudness instead. A meter that tracks the
// signal instantly is a peak meter wearing a VU's face, and these tests are what stop
// the ballistics quietly turning into that.

#include "engine/OutputMeter.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double sr = 44100.0;

    /** Feeds `seconds` of a constant-amplitude signal in `blockSize` chunks. */
    void feedSteady (OutputMeter& meter, int channel, float amplitude, double seconds, int blockSize = 512)
    {
        std::vector<float> block ((std::size_t) blockSize, amplitude);
        const int totalBlocks = (int) std::round (seconds * sr / (double) blockSize);
        for (int i = 0; i < totalBlocks; ++i)
            meter.processBlock (channel, block.data(), blockSize);
    }
}

class OutputMeterTest final : public juce::UnitTest
{
public:
    OutputMeterTest() : juce::UnitTest ("RollForge OutputMeter", testCategory) {}

    void runTest() override
    {
        beginTest ("silence reads far below the scale, and out-of-range channels are safe");
        {
            OutputMeter m;
            m.prepare (sr);
            expect (m.getVuDb (0) < -40.0f);
            expect (m.getPeakDbfs (0) < -100.0f);

            // A mono or multichannel device must never write past the stereo pair.
            std::vector<float> block (64, 1.0f);
            m.processBlock (-1, block.data(), 64);
            m.processBlock (5,  block.data(), 64);
            m.processBlock (0,  nullptr, 64);
            expect (m.getVuDb (0) < -40.0f, "an ignored block must not move the needle");
            expect (m.getVuDb (7) < -40.0f);
        }

        beginTest ("a steady tone at the 0 VU reference eventually reads 0 VU");
        {
            // A DC block of amplitude a has RMS a. 0 VU is defined at -18 dBFS.
            const float reference = std::pow (10.0f, OutputMeter::zeroVuDbfs / 20.0f);   // ~0.126

            OutputMeter m;
            m.prepare (sr);
            feedSteady (m, 0, reference, 2.0);   // well past the 300 ms integration
            expectWithinAbsoluteError (m.getVuDb (0), 0.0f, 0.1f);

            // And 6 dB hotter reads +6 VU.
            OutputMeter louder;
            louder.prepare (sr);
            feedSteady (louder, 0, reference * 2.0f, 2.0);
            expectWithinAbsoluteError (louder.getVuDb (0), 6.0f, 0.1f);
        }

        beginTest ("the needle reaches 99% of a step in 300 ms — no faster, no slower");
        {
            // This IS the definition of a VU meter's ballistics.
            const float target = 0.5f;

            auto readingAfter = [&] (double seconds)
            {
                OutputMeter m;
                m.prepare (sr);
                feedSteady (m, 0, target, seconds, 64);   // small blocks: fine time resolution
                return std::pow (10.0f, (m.getVuDb (0) + OutputMeter::zeroVuDbfs) / 20.0f);
            };

            const float at300ms = readingAfter (0.300);
            expectWithinAbsoluteError (at300ms / target, 0.99f, 0.02f,
                                       "at 300 ms a VU must be at 99% of the step");

            // Half way through it must be well short of the target — a fast meter would
            // already be pinned.
            const float at100ms = readingAfter (0.100);
            expect (at100ms / target < 0.85f, "the needle must not snap to the value");
            expect (at100ms / target > 0.60f, "...nor crawl");
        }

        beginTest ("the ballistics do not depend on the host's block size");
        {
            auto readAfter200ms = [&] (int blockSize)
            {
                OutputMeter m;
                m.prepare (sr);
                feedSteady (m, 0, 0.5f, 0.200, blockSize);
                return m.getVuDb (0);
            };

            const float b64   = readAfter200ms (64);
            const float b256  = readAfter200ms (256);
            const float b1024 = readAfter200ms (1024);

            expectWithinAbsoluteError (b64, b256, 0.3f);
            expectWithinAbsoluteError (b64, b1024, 0.6f);
        }

        beginTest ("a lone transient pins the peak but barely moves the VU");
        {
            // The whole reason a drum machine wants a VU: one full-scale kick should not
            // slam the needle, but it must light the peak lamp.
            OutputMeter m;
            m.prepare (sr);

            std::vector<float> block (512, 0.0f);
            block[0] = 1.0f;                     // a single full-scale sample
            m.processBlock (0, block.data(), 512);

            expectWithinAbsoluteError (m.getPeakDbfs (0), 0.0f, 0.01f, "peak sees full scale");
            expect (m.getVuDb (0) < -20.0f, "the needle is nearly unmoved by one sample");
        }

        beginTest ("peak has an instant attack and a slow release; VU falls back too");
        {
            OutputMeter m;
            m.prepare (sr);
            feedSteady (m, 0, 0.8f, 1.0);
            expectWithinAbsoluteError (m.getPeakDbfs (0), OutputMeter::toDbfs (0.8f), 0.01f);
            const float loudVu = m.getVuDb (0);
            expect (loudVu > 0.0f);

            feedSteady (m, 0, 0.0f, 0.5);        // silence for half a second
            expect (m.getPeakDbfs (0) < OutputMeter::toDbfs (0.8f) - 1.0f, "peak decays");
            expect (m.getPeakDbfs (0) > -30.0f, "...but slowly, so it stays readable");
            expect (m.getVuDb (0) < loudVu - 10.0f, "the needle falls back");
        }

        beginTest ("the needle deflects linearly in amplitude, not in decibels");
        {
            // A real VU's -20..0 marks are crowded and its 0..+3 marks are wide, because
            // the movement deflects with voltage. Driving the needle from dB (and printing
            // even ticks) is the single tell of a fake meter.
            expectWithinAbsoluteError (OutputMeter::deflectionForVuDb (3.0f), 1.0f, 0.001f,
                                       "+3 VU is full-scale deflection");
            expectWithinAbsoluteError (OutputMeter::deflectionForVuDb (0.0f), 1.0f / 1.413f, 0.002f,
                                       "0 VU sits at ~71% of the arc");
            expectWithinAbsoluteError (OutputMeter::deflectionForVuDb (-20.0f), 0.1f / 1.413f, 0.002f,
                                       "-20 VU sits at ~7% — crowded at the bottom");

            // Doubling the voltage doubles the deflection. That is the whole law.
            expectWithinAbsoluteError (OutputMeter::deflectionForVuDb (-6.0f) * 2.0f,
                                       OutputMeter::deflectionForVuDb (0.0f), 0.005f);

            // And the live needle agrees with the printed scale.
            const float reference = std::pow (10.0f, OutputMeter::zeroVuDbfs / 20.0f);
            OutputMeter m;
            m.prepare (sr);
            feedSteady (m, 0, reference, 2.0);
            expectWithinAbsoluteError (m.getVuDeflection (0),
                                       OutputMeter::deflectionForVuDb (0.0f), 0.01f);
        }

        beginTest ("the two channels are independent");
        {
            OutputMeter m;
            m.prepare (sr);
            feedSteady (m, 0, 0.5f, 1.0);
            expect (m.getVuDb (0) > 0.0f);
            expect (m.getVuDb (1) < -40.0f, "the right channel stayed silent");
        }
    }
};

static OutputMeterTest outputMeterTest;

} // namespace rollforge::tests
