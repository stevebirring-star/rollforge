// RollForge — DrumEngine seam unit tests.
//
// Headless (no audio device): drive the command -> audio-out path directly.
// Proves the seam is silent until triggered, audible after a queued trigger,
// additive (does not clobber the caller's buffer), and velocity-scaled.

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"

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

        beginTest ("mute / solo audibility logic");
        {
            DrumEngine engine;
            engine.prepare (sampleRate, blockSize);

            for (int p = 0; p < 16; ++p) expect (engine.isPadAudible (p));   // default: all audible

            engine.setPadMuted (2, true);
            expect (engine.isPadMuted (2));
            expect (! engine.isPadAudible (2));
            expect (engine.isPadAudible (0));

            // Any solo -> only soloed pads audible (solo overrides mute for the rest).
            engine.setPadSoloed (5, true);
            expect (engine.isPadSoloed (5));
            expect (engine.isPadAudible (5));
            expect (! engine.isPadAudible (0));   // not soloed
            expect (! engine.isPadAudible (2));   // not soloed

            // Redundant sets keep the solo count exact; one clear is enough.
            engine.setPadSoloed (5, true);
            engine.setPadSoloed (5, false);
            expect (engine.isPadAudible (0));     // back to mute-only semantics
            expect (! engine.isPadAudible (2));   // pad 2 still muted
            expect (engine.isPadAudible (5));

            expect (engine.isPadAudible (-1));    // out-of-range is never gated
            expect (engine.isPadAudible (999));
        }

        beginTest ("a muted pad is silent under the sequencer but still auditions");
        {
            const int block = 512;

            auto sequencedMag = [&] (bool mutePad0)
            {
                DrumEngine e; e.prepare (sampleRate, block);
                if (mutePad0) e.setPadMuted (0, true);

                Sequencer seq; seq.prepare (sampleRate);
                seq.setTempo (120.0);

                Pattern p; p.numLanes = 1;
                p.lane (0).targetPad   = 0;
                p.lane (0).length      = 16;
                p.lane (0).step (0).on = true;
                seq.setPattern (p);
                seq.requestReset();
                seq.setPlaying (true);

                juce::AudioBuffer<float> buf (2, block);
                float mag = 0.0f;
                for (int b = 0; b < 4; ++b)
                {
                    buf.clear();
                    seq.process (e, buf);
                    mag = juce::jmax (mag, buf.getMagnitude (0, 0, block));
                }
                return mag;
            };

            expect (sequencedMag (false) > 0.0f);                            // unmuted step sounds
            expectWithinAbsoluteError (sequencedMag (true), 0.0f, 1.0e-7f);   // muted step is silent

            // A manual audition (command path) bypasses mute so the sound is checkable.
            DrumEngine e; e.prepare (sampleRate, block);
            e.setPadMuted (0, true);
            e.pushTrigger (0, 1.0f);
            juce::AudioBuffer<float> buf (2, block); buf.clear();
            e.process (buf);
            expect (buf.getMagnitude (0, 0, block) > 0.0f);
        }

        beginTest ("the 17th (preview) pad sounds without disturbing the 16 kit pads");
        {
            // The library browser auditions through a pad past the kit, so previewing a
            // sample can't change what a kit pad plays, light its meter, or be gated by
            // mute/solo. Mirrors AudioEngine's DrumEngine { 1024, 64, previewPadIndex + 1 }.
            constexpr int previewPad = 16;
            DrumEngine engine { 1024, 64, previewPad + 1 };
            engine.prepare (sampleRate, blockSize);
            expectEquals (engine.getNumPads(), previewPad + 1);

            // Longer than one block, so the voice is provably still active afterwards.
            juce::AudioBuffer<float> audio (1, blockSize * 4);
            for (int i = 0; i < audio.getNumSamples(); ++i)
                audio.setSample (0, i, 0.5f);
            SampleBuffer::Ptr preview = new SampleBuffer (std::move (audio), sampleRate, "preview");

            engine.pushSetPad (previewPad, preview, VoiceParameters {}, 0);
            engine.pushTrigger (previewPad, 1.0f);

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            engine.process (buffer);

            expect (! isSilent (buffer), "the preview pad is audible");
            expect (engine.getNumActiveVoices() >= 1);

            // Meters cover the 16 kit pads only; the preview must not spill into them.
            for (int p = 0; p < 16; ++p)
                expectWithinAbsoluteError (engine.getPadLevel (p), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (engine.getPadLevel (previewPad), 0.0f, 1.0e-6f);

            // Soloing a kit pad must not silence the preview: gating applies to sequenced
            // hits, and isPadAudible never gates an index past the kit.
            engine.setPadSoloed (0, true);
            expect (engine.isPadAudible (previewPad));
        }
    }
};

static DrumEngineTest drumEngineTest;

} // namespace rollforge::tests
