// RollForge — VoicePool choke-group unit tests (Phase-1 acceptance gate).
//
// Headless checks of choke semantics: a non-zero choke group is mutually
// exclusive (a new trigger in the group stops the others), group 0 never chokes,
// different groups coexist, a choke stops only the matching group, and a choked
// slot is freed for reuse.

#include "engine/VoicePool.h"

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    SampleBuffer::Ptr makeSample (int n = 8)
    {
        juce::AudioBuffer<float> audio (1, n);
        for (int i = 0; i < n; ++i)
            audio.setSample (0, i, 0.5f);
        return new SampleBuffer (std::move (audio), 44100.0, "s");
    }
}

class ChokeGroupTest final : public juce::UnitTest
{
public:
    ChokeGroupTest() : juce::UnitTest ("RollForge ChokeGroups", testCategory) {}

    void runTest() override
    {
        auto sample = makeSample();
        const Voice::Parameters params;

        beginTest ("same choke group is mutually exclusive");
        {
            VoicePool pool (8);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f, /*chokeGroup*/ 1);
            expectEquals (pool.getNumActive(), 1);
            pool.trigger (sample, params, 1.0f, 1);   // chokes the first
            expectEquals (pool.getNumActive(), 1);
            pool.trigger (sample, params, 1.0f, 1);   // still just one
            expectEquals (pool.getNumActive(), 1);
        }

        beginTest ("group 0 does not choke");
        {
            VoicePool pool (8);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f, 0);
            pool.trigger (sample, params, 1.0f, 0);
            expectEquals (pool.getNumActive(), 2);
        }

        beginTest ("different choke groups coexist");
        {
            VoicePool pool (8);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f, 1);
            pool.trigger (sample, params, 1.0f, 2);
            expectEquals (pool.getNumActive(), 2);
        }

        beginTest ("a choke stops only the matching group");
        {
            VoicePool pool (8);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f, 1);   // A (group 1)
            pool.trigger (sample, params, 1.0f, 2);   // B (group 2)
            pool.trigger (sample, params, 1.0f, 1);   // C (group 1) chokes A, leaves B
            expectEquals (pool.getNumActive(), 2);
        }

        beginTest ("a choked slot is freed for reuse");
        {
            VoicePool pool (2);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f, 1);   // A (group 1)
            pool.trigger (sample, params, 1.0f, 1);   // B chokes A, reuses the slot
            expectEquals (pool.getNumActive(), 1);
            pool.trigger (sample, params, 1.0f, 2);   // group 2 takes the free slot
            expectEquals (pool.getNumActive(), 2);
        }
    }
};

static ChokeGroupTest chokeGroupTest;

} // namespace rollforge::tests
