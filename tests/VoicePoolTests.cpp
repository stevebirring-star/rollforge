// RollForge — VoicePool voice-stealing unit tests (Phase-1 acceptance gate).
//
// Headless, deterministic checks of allocation + stealing: idle voices are used
// before any steal; an idle slot is reused rather than stolen; a full pool steals
// the QUIETEST voice; and steal ties break to the OLDEST voice. "Quietest" is the
// tracked level (envelope*gain*velocity), read before rendering, so results are
// exact with no audio device.

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

class VoicePoolTest final : public juce::UnitTest
{
public:
    VoicePoolTest() : juce::UnitTest ("RollForge VoicePool", testCategory) {}

    void runTest() override
    {
        auto sample = makeSample();
        const Voice::Parameters params;   // defaults: no envelope, gain 1, centre

        beginTest ("fills idle voices before stealing");
        {
            VoicePool pool (4);
            pool.prepare (44100.0);
            expectEquals (pool.getNumVoices(), 4);
            expectEquals (pool.getNumActive(), 0);

            for (int i = 0; i < 4; ++i)
                pool.trigger (sample, params, 1.0f);

            expectEquals (pool.getNumActive(), 4);
            for (int i = 0; i < 4; ++i)
                expect (pool.isVoiceActive (i));
        }

        beginTest ("reuses an idle slot instead of stealing");
        {
            VoicePool pool (4);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 1.0f);      // slot 0
            expect (pool.isVoiceActive (0));

            // Render past the 8-sample source so slot 0 goes idle.
            juce::AudioBuffer<float> buf (1, 64);
            buf.clear();
            pool.renderAdditive (buf, 0, 64);
            expectEquals (pool.getNumActive(), 0);

            pool.trigger (sample, params, 1.0f);      // reuses idle slot 0
            expect (pool.isVoiceActive (0));
            expectEquals (pool.getNumActive(), 1);
        }

        beginTest ("steals the quietest voice when full");
        {
            VoicePool pool (4);
            pool.prepare (44100.0);
            const float velocities[4] = { 0.9f, 0.2f, 0.7f, 0.5f };
            for (int i = 0; i < 4; ++i)
                pool.trigger (sample, params, velocities[i]);

            expectWithinAbsoluteError (pool.getVoiceLevel (1), 0.2f, 1.0e-4f);   // quietest

            pool.trigger (sample, params, 1.0f);      // full -> steals slot 1
            expectEquals (pool.getNumActive(), 4);
            expectWithinAbsoluteError (pool.getVoiceLevel (1), 1.0f, 1.0e-4f);   // reassigned
            expectWithinAbsoluteError (pool.getVoiceLevel (0), 0.9f, 1.0e-4f);   // others untouched
            expectWithinAbsoluteError (pool.getVoiceLevel (2), 0.7f, 1.0e-4f);
            expectWithinAbsoluteError (pool.getVoiceLevel (3), 0.5f, 1.0e-4f);
        }

        beginTest ("breaks steal ties by oldest voice");
        {
            VoicePool pool (2);
            pool.prepare (44100.0);
            pool.trigger (sample, params, 0.5f);      // slot 0, age 0
            pool.trigger (sample, params, 0.5f);      // slot 1, age 1
            expect (pool.getVoiceAge (0) < pool.getVoiceAge (1));

            // Equal levels -> steal the OLDEST (slot 0), which then gets a new age.
            pool.trigger (sample, params, 0.5f);
            expectEquals (pool.getNumActive(), 2);
            expect (pool.getVoiceAge (0) > pool.getVoiceAge (1));
        }
    }
};

static VoicePoolTest voicePoolTest;

} // namespace rollforge::tests
