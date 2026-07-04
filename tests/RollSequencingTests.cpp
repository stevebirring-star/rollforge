// RollForge — roll-through-Sequencer tests (Phase 3, commit 2).
//
// Headless: a Pattern carrying a CompiledRoll fires the roll's hits through the
// Sequencer at the right (scaled) sample positions. No kit -> the interim blip
// makes fired hits audible.

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"
#include "model/RollCompiler.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    bool isSilent (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            if (b.getMagnitude (ch, 0, b.getNumSamples()) > 0.0f)
                return false;
        return true;
    }

    Pattern patternWithRoll (const RollRegion& region)
    {
        Pattern p;
        p.numLanes = 0;               // only the roll fires
        p.rolls[0] = RollCompiler::compile (region);
        p.numRolls = 1;
        return p;
    }
}

class RollSequencingTest final : public juce::UnitTest
{
public:
    RollSequencingTest() : juce::UnitTest ("RollForge RollSequencing", testCategory) {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("a pattern roll fires its compiled hits");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            RollRegion r;
            r.startStep = 0.0; r.lengthSteps = 1.0; r.targetPad = 0;
            r.speed = { 4.0f, 4.0f, 0.0f };   // 4 hits over 1 step
            r.volume = { 1.0f, 1.0f, 0.0f };
            seq.setPattern (patternWithRoll (r));
            seq.setPlaying (true);

            // One step (~5512 samples) contains the roll's 4 hits; step 16 (its next
            // firing) is far past this window, so exactly 4 fire.
            const int total = 5600;
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

        beginTest ("a roll's first hit is sample-accurate and audible");
        {
            DrumEngine engine;
            engine.prepare (sr, 16384);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);

            RollRegion r;
            r.startStep = 1.0; r.lengthSteps = 1.0; r.targetPad = 0;
            r.speed = { 2.0f, 2.0f, 0.0f };   // 2 hits over 1 step
            r.volume = { 1.0f, 1.0f, 0.0f };
            seq.setPattern (patternWithRoll (r));
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 16384);
            buf.clear();
            seq.process (engine, buf);

            // Roll starts at step 1 -> ~5513 samples: silent before, audible after.
            expectWithinAbsoluteError (buf.getMagnitude (0, 0, 5000), 0.0f, 0.0f);
            expect (! isSilent (buf));
        }
    }
};

static RollSequencingTest rollSequencingTest;

} // namespace rollforge::tests
