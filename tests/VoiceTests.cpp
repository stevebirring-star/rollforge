// RollForge — Voice unit tests.
//
// Headless checks of single-voice playback: idle silence, unity reproduction of
// the source, equal-power pan, reverse, pitch (octave-up halves the duration),
// and the AR attack ramp. Device rate == native rate (44100) so the pitch-0
// read increment is exactly 1.0 and output frames map 1:1 to source samples.

#include "engine/Voice.h"

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double kRate = 44100.0;

    // Ramp source: sample i holds (i+1)/n, so forward vs reverse is visible and
    // every sample is distinct. Mono unless `channels` says otherwise.
    SampleBuffer::Ptr makeRamp (int n, int channels = 1)
    {
        juce::AudioBuffer<float> audio (channels, n);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < n; ++i)
                audio.setSample (ch, i, (float) (i + 1) / (float) n);

        return new SampleBuffer (std::move (audio), kRate, "ramp");
    }

    const float kCentre = std::sqrt (0.5f);   // equal-power centre gain ~= 0.70711
}

class VoiceTest final : public juce::UnitTest
{
public:
    VoiceTest() : juce::UnitTest ("RollForge Voice", testCategory) {}

    void runTest() override
    {
        beginTest ("idle voice renders silence");
        {
            Voice v;
            v.prepare (kRate);
            expect (! v.isActive());

            juce::AudioBuffer<float> buf (2, 64);
            buf.clear();
            v.renderAdditive (buf, 0, 64);

            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 64), 0.0f, 0.0f);
            expectWithinAbsoluteError (buf.getMagnitude (1, 0, 64), 0.0f, 0.0f);
        }

        beginTest ("unity playback reproduces the source, then idles");
        {
            Voice v;
            v.prepare (kRate);
            auto src = makeRamp (32);
            v.start (src, {}, 1.0f);          // default params: gain 1, centre, no env
            expect (v.isActive());

            juce::AudioBuffer<float> buf (2, 64);
            buf.clear();
            v.renderAdditive (buf, 0, 64);    // more than the 32 source frames

            for (int i = 0; i < 32; ++i)
            {
                const float expected = src->getSample (0, i) * kCentre;
                expectWithinAbsoluteError (buf.getSample (0, i), expected, 1.0e-4f);
                expectWithinAbsoluteError (buf.getSample (1, i), expected, 1.0e-4f);
            }
            // Past the sample end: silent, and the voice has gone idle.
            expectWithinAbsoluteError (buf.getSample (0, 40), 0.0f, 1.0e-6f);
            expect (! v.isActive());
        }

        beginTest ("hard-left pan silences the right channel");
        {
            Voice v;
            v.prepare (kRate);
            auto src = makeRamp (16);
            Voice::Parameters p; p.pan = -1.0f;
            v.start (src, p, 1.0f);

            juce::AudioBuffer<float> buf (2, 16);
            buf.clear();
            v.renderAdditive (buf, 0, 16);

            expectWithinAbsoluteError (buf.getSample (0, 5), src->getSample (0, 5), 1.0e-4f); // left = full
            expectWithinAbsoluteError (buf.getMagnitude (1, 0, 16), 0.0f, 1.0e-6f);           // right silent
        }

        beginTest ("hard-right pan silences the left channel");
        {
            Voice v;
            v.prepare (kRate);
            auto src = makeRamp (16);
            Voice::Parameters p; p.pan = 1.0f;
            v.start (src, p, 1.0f);

            juce::AudioBuffer<float> buf (2, 16);
            buf.clear();
            v.renderAdditive (buf, 0, 16);

            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 16), 0.0f, 1.0e-6f);           // left silent
            expectWithinAbsoluteError (buf.getSample (1, 5), src->getSample (0, 5), 1.0e-4f); // right = full
        }

        beginTest ("reverse plays the sample backwards");
        {
            Voice v;
            v.prepare (kRate);
            const int n = 16;
            auto src = makeRamp (n);
            Voice::Parameters p; p.reverse = true;
            v.start (src, p, 1.0f);

            juce::AudioBuffer<float> buf (2, n);
            buf.clear();
            v.renderAdditive (buf, 0, n);

            // First output frame is the LAST source sample.
            expectWithinAbsoluteError (buf.getSample (0, 0), src->getSample (0, n - 1) * kCentre, 1.0e-4f);
            expectWithinAbsoluteError (buf.getSample (0, 1), src->getSample (0, n - 2) * kCentre, 1.0e-4f);
        }

        beginTest ("pitch up an octave halves the playback duration");
        {
            Voice v;
            v.prepare (kRate);
            auto src = makeRamp (32);
            Voice::Parameters p; p.pitchSemitones = 12.0f;   // 2x speed
            v.start (src, p, 1.0f);

            juce::AudioBuffer<float> buf (1, 64);
            buf.clear();
            v.renderAdditive (buf, 0, 64);

            // ~16 frames produced (floor(31/2)+1 = 16): frame 10 sounds, 20 is silent.
            expect (std::abs (buf.getSample (0, 10)) > 0.0f);
            expectWithinAbsoluteError (buf.getSample (0, 20), 0.0f, 1.0e-6f);
            expect (! v.isActive());
            // Second output frame reads source position 2 (skipping one sample).
            expectWithinAbsoluteError (buf.getSample (0, 1), src->getSample (0, 2) * v_monoCentre(), 1.0e-4f);
        }

        beginTest ("attack ramp starts silent and rises to full");
        {
            Voice v;
            v.prepare (kRate);
            const int n = 256;
            auto src = makeRamp (n);
            Voice::Parameters p;
            p.attackMs = 1000.0f * 44.0f / (float) kRate;   // ~44-frame attack
            v.start (src, p, 1.0f);

            juce::AudioBuffer<float> buf (1, n);
            buf.clear();
            v.renderAdditive (buf, 0, n);

            // Frame 0: envelope 0 -> silent regardless of source value.
            expectWithinAbsoluteError (buf.getSample (0, 0), 0.0f, 1.0e-6f);
            // Rising through the attack.
            expect (std::abs (buf.getSample (0, 30)) > std::abs (buf.getSample (0, 10)));
            // Well past the attack: full level.
            expectWithinAbsoluteError (buf.getSample (0, 100), src->getSample (0, 100) * v_monoCentre(), 1.0e-3f);
        }
    }

private:
    // Mono-output centre gain used by the Voice (0.5*(left+right) at pan 0).
    static float v_monoCentre() { return std::sqrt (0.5f); }
};

static VoiceTest voiceTest;

} // namespace rollforge::tests
