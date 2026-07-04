// RollForge — Sequencer unit tests (Phase 2, commit 3).
//
// Headless: drive the Sequencer + DrumEngine directly. With no kit installed,
// triggered pads fall back to the interim blip, so a fired step makes the output
// non-silent from its exact sample offset — which is how we check sample-accurate
// timing without an audio device.

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    // One lane, one step on, targeting `pad`.
    Pattern onePadPattern (int pad, int length, int stepOn)
    {
        Pattern p;
        p.numLanes = 1;
        p.lane (0).targetPad = pad;
        p.lane (0).length = length;
        p.lane (0).step (stepOn).on = true;
        p.lane (0).step (stepOn).velocity = 1.0f;
        return p;
    }
}

class SequencerTest final : public juce::UnitTest
{
public:
    SequencerTest() : juce::UnitTest ("RollForge Sequencer", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("fires a step at the exact sample offset");
        {
            DrumEngine engine;
            engine.prepare (sr, 16384);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPattern (onePadPattern (0, 16, /*stepOn*/ 1));   // fires at step 1
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 16384);
            buf.clear();
            seq.process (engine, buf);

            // step 1 @ 120/44100 = round(1 * 5512.5) = 5513: silent before, audible after.
            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 5000), 0.0f, 0.0f);
            expect (buf.getMagnitude (0, 5513, 3000) > 0.0f);
            expectEquals (seq.getTriggerCount(), (std::int64_t) 1);
        }

        beginTest ("no triggers while stopped");
        {
            DrumEngine engine;
            engine.prepare (sr, 8192);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPattern (onePadPattern (0, 16, 0));
            seq.setPlaying (false);

            juce::AudioBuffer<float> buf (1, 8192);
            buf.clear();
            seq.process (engine, buf);

            expectEquals (seq.getTriggerCount(), (std::int64_t) 0);
            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 8192), 0.0f, 0.0f);
        }

        beginTest ("an empty pattern fires nothing");
        {
            DrumEngine engine;
            engine.prepare (sr, 8192);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPlaying (true);   // default pattern has numLanes == 0

            juce::AudioBuffer<float> buf (1, 8192);
            buf.clear();
            seq.process (engine, buf);

            expectEquals (seq.getTriggerCount(), (std::int64_t) 0);
        }

        beginTest ("a lane loops on its length");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPattern (onePadPattern (0, /*length*/ 4, /*stepOn*/ 0));
            seq.setPlaying (true);

            // 8 steps' worth = 8 * 5512.5 = 44100 samples. Steps 0..7 fire; pos =
            // G % 4 == 0 for G = 0 and G = 4 -> exactly 2 triggers.
            const int total = 44100;
            int done = 0;
            while (done < total)
            {
                const int n = juce::jmin (512, total - done);
                juce::AudioBuffer<float> buf (1, n);
                buf.clear();
                seq.process (engine, buf);
                done += n;
            }

            expectEquals (seq.getTriggerCount(), (std::int64_t) 2);
        }

        beginTest ("multiple lanes trigger their own pads in one step");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            Pattern p;
            p.numLanes = 2;
            p.lane (0).targetPad = 0; p.lane (0).length = 16; p.lane (0).step (0).on = true;
            p.lane (1).targetPad = 1; p.lane (1).length = 16; p.lane (1).step (0).on = true;
            seq.setPattern (p);
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 512);
            buf.clear();
            seq.process (engine, buf);   // covers step 0 at sample 0

            expectEquals (seq.getTriggerCount(), (std::int64_t) 2);
            expectEquals (engine.getNumActiveVoices(), 2);
        }

        beginTest ("ratchets fire N evenly-spaced sub-hits across a step");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (0).ratchets = 4;
            seq.setPattern (p);
            seq.setPlaying (true);

            // One step of ~5512 samples holds all 4 ratchet hits (at 0, ~1378, ~2756, ~4134).
            const int total = 6000;
            int done = 0;
            while (done < total)
            {
                const int n = juce::jmin (512, total - done);
                juce::AudioBuffer<float> buf (1, n);
                buf.clear();
                seq.process (engine, buf);
                done += n;
            }
            expectEquals (seq.getTriggerCount(), (std::int64_t) 4);
        }

        beginTest ("probability gates firing deterministically");
        {
            auto countWith = [&] (int probability)
            {
                DrumEngine engine;
                engine.prepare (sr, 512);
                Sequencer seq;
                seq.prepare (sr);
                seq.setTempo (120.0);

                Pattern p;
                p.numLanes = 1;
                p.lane (0).targetPad = 0; p.lane (0).length = 1;   // every global step fires
                p.lane (0).step (0).on = true;
                p.lane (0).step (0).probability = probability;
                seq.setPattern (p);
                seq.setPlaying (true);

                const int total = (int) std::llround (64 * 5512.5);   // 64 global steps
                int done = 0;
                while (done < total)
                {
                    const int n = juce::jmin (512, total - done);
                    juce::AudioBuffer<float> buf (1, n);
                    buf.clear();
                    seq.process (engine, buf);
                    done += n;
                }
                return seq.getTriggerCount();
            };

            expectEquals (countWith (100), (std::int64_t) 64);   // always
            expectEquals (countWith (0),   (std::int64_t) 0);    // never
            const auto quarter = countWith (25);
            expect (quarter > 0 && quarter < 64);                // some, but fewer
            expectEquals (countWith (25), quarter);              // deterministic
        }

        beginTest ("micro-shift moves a step off the grid");
        {
            DrumEngine engine;
            engine.prepare (sr, 16384);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (0).microShift = 0.5f;   // +half a step
            seq.setPattern (p);
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 16384);
            buf.clear();
            seq.process (engine, buf);

            // step 0 grid @ 0 -> +0.5 * 5512.5 = ~2756: silent before, audible after.
            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 2500), 0.0f, 0.0f);
            expect (buf.getMagnitude (0, 2756, 3000) > 0.0f);
            expectEquals (seq.getTriggerCount(), (std::int64_t) 1);
        }

        beginTest ("backward micro-shift is clamped to the grid (forward-only for now)");
        {
            DrumEngine engine;
            engine.prepare (sr, 16384);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (0).on = true;
            p.lane (0).step (0).microShift = -0.5f;   // negative -> clamped to 0
            seq.setPattern (p);
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 16384);
            buf.clear();
            seq.process (engine, buf);

            // Clamped to 0 -> fires on the grid at sample 0 (audible from the start).
            expect (buf.getMagnitude (0, 0, 2000) > 0.0f);
            expectEquals (seq.getTriggerCount(), (std::int64_t) 1);
        }

        beginTest ("swing delays the off-beat 1/16 steps");
        {
            DrumEngine engine;
            engine.prepare (sr, 16384);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            Pattern p;
            p.numLanes = 1;
            p.lane (0).targetPad = 0; p.lane (0).length = 16;
            p.lane (0).step (1).on = true;   // step 1 is an off-beat 1/16
            seq.setPattern (p);
            seq.setSwing (1.0f);
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 16384);
            buf.clear();
            seq.process (engine, buf);

            // grid 5513 + swing (5512.5/3 ~= 1838) -> ~7351: silent before, audible after.
            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 7000), 0.0f, 0.0f);
            expect (buf.getMagnitude (0, 7351, 2000) > 0.0f);
            expectEquals (seq.getTriggerCount(), (std::int64_t) 1);
        }
    }
};

static SequencerTest sequencerTest;

} // namespace rollforge::tests
