// RollForge — DrumEngine seam unit tests.
//
// Headless (no audio device): drive the command -> audio-out path directly.
// Proves the seam is silent until triggered, audible after a queued trigger,
// additive (does not clobber the caller's buffer), and velocity-scaled.

#include "engine/DrumEngine.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    bool isSilent (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            if (buffer.getMagnitude (ch, 0, buffer.getNumSamples()) > 0.0f)
                return false;
        return true;
    }
}

class DrumEngineTest final : public juce::UnitTest
{
public:
    DrumEngineTest() : juce::UnitTest ("RollForge DrumEngine", testCategory) {}

    void runTest() override
    {
        const double sampleRate = 44100.0;
        const int    blockSize  = 256;

        beginTest ("no output without a trigger");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);

            expect (isSilent (buffer));
        }

        beginTest ("a queued trigger produces audible output");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);
            expect (engine.pushTrigger (0, 1.0f));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);

            expect (! isSilent (buffer));
        }

        beginTest ("idle process is additive (leaves an existing buffer untouched)");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);

            juce::AudioBuffer<float> buffer (2, blockSize);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.5f, blockSize);

            engine.process (buffer);   // no trigger queued -> adds nothing

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const auto* d = buffer.getReadPointer (ch);
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError (d[i], 0.5f, 1.0e-6f);
            }
        }

        beginTest ("velocity scales the trigger level");
        {
            auto peakForVelocity = [&] (float velocity)
            {
                DrumEngine engine;
                engine.prepare (sampleRate, blockSize);
                engine.pushTrigger (0, velocity);

                juce::AudioBuffer<float> buffer (1, blockSize);
                buffer.clear();
                engine.process (buffer);
                return buffer.getMagnitude (0, 0, blockSize);
            };

            const float loud  = peakForVelocity (1.0f);
            const float quiet = peakForVelocity (0.25f);

            expect (loud > 0.0f);
            expect (loud > quiet);
            // Gain is linear in velocity, so quiet ~= 0.25 * loud (same waveform).
            expectWithinAbsoluteError (quiet, loud * 0.25f, loud * 0.02f + 1.0e-5f);
        }

        beginTest ("a MIDI-queued trigger produces audible output");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);
            expect (engine.pushMidiTrigger (0, 1.0f));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);

            expect (! isSilent (buffer));
        }

        beginTest ("message and MIDI triggers both play in one block");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.pushTrigger (0, 1.0f);        // message queue
            engine.pushMidiTrigger (1, 1.0f);    // MIDI queue

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);             // drains both queues

            // No kit installed -> both fall back to the interim blip (choke 0),
            // so both triggers occupy voices.
            expectEquals (engine.getNumActiveVoices(), 2);
        }

        beginTest ("per-pad level meter tracks only the triggered pad");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);

            // Nothing triggered -> every pad meter reads 0; out-of-range is 0 too.
            expectWithinAbsoluteError (engine.getPadLevel (0), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (engine.getPadLevel (3), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (engine.getPadLevel (-1), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (engine.getPadLevel (999), 0.0f, 1.0e-6f);

            engine.pushTrigger (3, 1.0f);
            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);   // drains, renders, publishes the meters

            expect (engine.getPadLevel (3) > 0.0f);                            // hit pad lights up
            expectWithinAbsoluteError (engine.getPadLevel (7), 0.0f, 1.0e-6f);  // untouched pads stay dark
        }
    }
};

static DrumEngineTest drumEngineTest;

} // namespace rollforge::tests
