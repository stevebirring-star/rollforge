// RollForge — per-lane triplet tests (Credibility tier).
//
// Two halves. The pure half checks model/Lane.h's rate arithmetic, including the
// invariant everything else rests on: a STRAIGHT lane's mapping is the identity, so the
// triplet machinery cannot change a single straight-lane sample. The engine half drives
// the Sequencer headlessly and reads back the exact sample each hit lands on, the same
// way SequencerTests does — an unconfigured pad plays the interim blip, so the first
// non-silent sample IS the trigger position.

#include "engine/DrumEngine.h"
#include "engine/Sequencer.h"
#include "model/MidiExporter.h"

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double sr = 44100.0;

    /** A short DC pulse. The engine's fallback blip is a decaying SINE, so it crosses
        zero repeatedly and a naive onset scan reads each crossing as a new hit. A DC
        pulse has no internal zeros, so "first non-silent sample" really is the trigger. */
    SampleBuffer::Ptr dcPulse (int n = 32)
    {
        juce::AudioBuffer<float> audio (1, n);
        for (int i = 0; i < n; ++i)
            audio.setSample (0, i, 0.5f);
        return new SampleBuffer (std::move (audio), sr, "pulse");
    }

    void installPulse (DrumEngine& engine)
    {
        static SampleBuffer::Ptr pulse = dcPulse();   // outlives every test
        engine.pushSetPad (0, pulse, VoiceParameters {}, /*chokeGroup*/ 0);
    }

    /** Sample indices at which the buffer first becomes non-silent after each silence. */
    std::vector<int> onsetSamples (const juce::AudioBuffer<float>& buffer)
    {
        std::vector<int> onsets;
        const float* d = buffer.getReadPointer (0);
        bool wasSilent = true;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const bool silent = std::abs (d[i]) < 1.0e-6f;
            if (wasSilent && ! silent)
                onsets.push_back (i);
            wasSilent = silent;
        }
        return onsets;
    }

    Pattern tripletLanePattern()
    {
        Pattern p;
        p.numLanes = 1;
        p.lane (0).targetPad = 0;
        p.lane (0).triplet   = true;
        p.lane (0).length    = tripletStepsPerBar;   // 12
        for (int s = 0; s < tripletStepsPerBar; ++s)
        {
            p.lane (0).step (s).on       = true;
            p.lane (0).step (s).velocity = 1.0f;
        }
        return p;
    }
}

class TripletTest final : public juce::UnitTest
{
public:
    TripletTest() : juce::UnitTest ("RollForge Triplets", testCategory) {}

    void runTest() override
    {
        beginTest ("a straight lane's rate mapping is the identity (nothing else may move)");
        {
            Lane straight;
            expect (! straight.triplet);
            expectEquals (laneStepRate (straight).num, 1);
            expectEquals (laneStepRate (straight).den, 1);
            expectEquals (defaultLaneLength (false), 16);

            for (std::int64_t g = 0; g < 64; ++g)
            {
                std::int64_t first = 0, end = 0;
                laneStepsInGlobalStep (straight, g, first, end);
                expectEquals ((int) first, (int) g, "exactly one lane step per global step");
                expectEquals ((int) end, (int) g + 1);
                expectWithinAbsoluteError (laneStepOffset (straight, g, g), 0.0, 1.0e-12);
                expectEquals ((int) laneStepAtGlobalStep (straight, g), (int) g);
            }
        }

        beginTest ("a triplet lane fires three steps per quarter, twelve per bar");
        {
            Lane triplet;
            triplet.triplet = true;
            expectEquals (laneStepRate (triplet).num, 4);
            expectEquals (laneStepRate (triplet).den, 3);
            expectEquals (defaultLaneLength (true), 12);

            // Over one bar of 16 global steps: 12 lane steps, none in global step 3, 7, 11, 15.
            int fired = 0;
            for (std::int64_t g = 0; g < 16; ++g)
            {
                std::int64_t first = 0, end = 0;
                laneStepsInGlobalStep (triplet, g, first, end);
                const int count = (int) (end - first);
                expect (count == 0 || count == 1, "a 1/8-triplet never fires twice in a 1/16");
                expectEquals (count, (g % 4 == 3) ? 0 : 1,
                              "global step " + juce::String ((int) g) + " fired " + juce::String (count));
                fired += count;

                for (std::int64_t p = first; p < end; ++p)
                {
                    const double offset = laneStepOffset (triplet, p, g);
                    expect (offset >= 0.0 && offset < 1.0, "the offset stays inside its global step");
                    // Lane step p sits at p * 4/3 sixteenths, absolutely.
                    expectWithinAbsoluteError ((double) g + offset, (double) p * 4.0 / 3.0, 1.0e-9);
                }
            }
            expectEquals (fired, 12, "twelve 1/8-triplets to the bar");

            // ... and the bar realigns exactly, so PatternBank + rolls stay correct.
            std::int64_t first = 0, end = 0;
            laneStepsInGlobalStep (triplet, 16, first, end);
            expectEquals ((int) first, 12);
            expectWithinAbsoluteError (laneStepOffset (triplet, 12, 16), 0.0, 1.0e-12);
        }

        beginTest ("the Sequencer places triplets at the exact sample of a 1/8 triplet");
        {
            DrumEngine engine;
            engine.prepare (sr, 1 << 16);
            installPulse (engine);
            Sequencer seq;
            seq.prepare (sr);
            seq.setTempo (120.0);
            seq.setPattern (tripletLanePattern());
            seq.setPlaying (true);

            juce::AudioBuffer<float> buf (1, 1 << 16);
            buf.clear();
            seq.process (engine, buf);

            const double samplesPerStep     = (sr * 60.0 / 120.0) / 4.0;   // 5512.5
            const double samplesPerTriplet  = samplesPerStep * 4.0 / 3.0;  // 7350

            const auto onsets = onsetSamples (buf);
            expect (onsets.size() >= 8, "expected at least 8 triplets in this block, got "
                                            + juce::String ((int) onsets.size()));

            for (std::size_t i = 0; i < onsets.size(); ++i)
            {
                const int expected = (int) std::llround ((double) i * samplesPerTriplet);
                expect (std::abs (onsets[i] - expected) <= 1,
                        "triplet " + juce::String ((int) i) + " at " + juce::String (onsets[i])
                            + ", expected " + juce::String (expected));
            }
        }

        beginTest ("triplet placement is drift-free across buffer sizes");
        {
            // The same invariant ClockTimingTests defends for the global grid: which lane
            // steps fire is integer maths, and each is anchored to the Clock's drift-free
            // step sample, so the block size cannot move a hit.
            auto onsetsWithBlockSize = [] (int blockSize)
            {
                DrumEngine engine;
                engine.prepare (sr, blockSize);
                installPulse (engine);
                Sequencer seq;
                seq.prepare (sr);
                seq.setTempo (120.0);
                seq.setPattern (tripletLanePattern());
                seq.setPlaying (true);

                const int total = 1 << 16;
                juce::AudioBuffer<float> full (1, total);
                full.clear();
                for (int done = 0; done < total; done += blockSize)
                {
                    const int n = juce::jmin (blockSize, total - done);
                    juce::AudioBuffer<float> block (full.getArrayOfWritePointers(), 1, done, n);
                    seq.process (engine, block);
                }
                return onsetSamples (full);
            };

            const auto a = onsetsWithBlockSize (64);
            const auto b = onsetsWithBlockSize (256);
            const auto c = onsetsWithBlockSize (1024);

            expect (! a.empty());
            expectEquals ((int) a.size(), (int) b.size());
            expectEquals ((int) a.size(), (int) c.size());
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                expectEquals (a[i], b[i], "64 vs 256 differ at triplet " + juce::String ((int) i));
                expectEquals (a[i], c[i], "64 vs 1024 differ at triplet " + juce::String ((int) i));
            }
        }

        beginTest ("a triplet lane does not swing (swing is a straight-1/16 idea)");
        {
            auto onsetsWithSwing = [] (float swing)
            {
                DrumEngine engine;
                engine.prepare (sr, 1 << 16);
                installPulse (engine);
                Sequencer seq;
                seq.prepare (sr);
                seq.setTempo (120.0);
                seq.setSwing (swing);
                seq.setPattern (tripletLanePattern());
                seq.setPlaying (true);

                juce::AudioBuffer<float> buf (1, 1 << 16);
                buf.clear();
                seq.process (engine, buf);
                return onsetSamples (buf);
            };

            const auto dry = onsetsWithSwing (0.0f);
            const auto wet = onsetsWithSwing (1.0f);
            expectEquals ((int) dry.size(), (int) wet.size());
            for (std::size_t i = 0; i < dry.size(); ++i)
                expectEquals (dry[i], wet[i], "swing must not move a triplet");
        }

        beginTest ("MIDI export puts triplets on triplet ticks, not the 1/16 grid");
        {
            Pattern p = tripletLanePattern();
            p.bpm = 120.0;

            constexpr int ppq = 960;
            const auto seq = MidiExporter::toSequence (p, /*bars*/ 1, ppq);

            std::vector<double> noteOnTicks;
            for (int i = 0; i < seq.getNumEvents(); ++i)
                if (seq.getEventPointer (i)->message.isNoteOn())
                    noteOnTicks.push_back (seq.getEventPointer (i)->message.getTimeStamp());

            expectEquals ((int) noteOnTicks.size(), 12, "twelve triplets in the bar");

            const double tripletTicks = (double) ppq / 3.0;   // a 1/8 triplet = a third of a quarter
            for (std::size_t i = 0; i < noteOnTicks.size(); ++i)
                expectWithinAbsoluteError (noteOnTicks[i], (double) i * tripletTicks, 1.0,
                                           "note " + juce::String ((int) i) + " is off the triplet grid");
        }

        beginTest ("a straight lane exports exactly as it did before triplets existed");
        {
            Pattern p;
            p.numLanes = 1; p.bpm = 120.0;
            p.lane (0).targetPad = 0;
            p.lane (0).length = 16;
            for (int s = 0; s < 16; s += 2)
                p.lane (0).step (s).on = true;

            constexpr int ppq = 960;
            const auto seq = MidiExporter::toSequence (p, 1, ppq);

            std::vector<double> ticks;
            for (int i = 0; i < seq.getNumEvents(); ++i)
                if (seq.getEventPointer (i)->message.isNoteOn())
                    ticks.push_back (seq.getEventPointer (i)->message.getTimeStamp());

            expectEquals ((int) ticks.size(), 8);
            for (std::size_t i = 0; i < ticks.size(); ++i)
                expectWithinAbsoluteError (ticks[i], (double) i * 2.0 * (ppq / 4.0), 1.0e-6);
        }
    }
};

static TripletTest tripletTest;

} // namespace rollforge::tests
