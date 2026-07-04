// RollForge — SampleBuffer + SampleRetirementPool unit tests.
//
// These prove the real-time reclamation contract described in SampleBuffer.h:
// a retired buffer is NOT freed while a Voice still references it, and IS freed
// by a later sweep once the Voice has released it — so the final delete never
// needs to run on the audio thread. All headless: no audio device required.

#include "engine/SampleBuffer.h"
#include "engine/SampleRetirementPool.h"

namespace rollforge::tests
{

// Every RollForge test registers under this category (see TestMain.cpp).
static constexpr const char* testCategory = "rollforge";

namespace
{
    /** A ramp-filled test buffer so getSample() reads are verifiable. */
    SampleBuffer::Ptr makeTestBuffer (int channels = 2, int samples = 8)
    {
        juce::AudioBuffer<float> audio (channels, samples);
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < samples; ++i)
                audio.setSample (ch, i, (float) (ch * 100 + i));

        return new SampleBuffer (std::move (audio), 48000.0, "test");
    }

    /** A SampleBuffer that flips an external flag in its destructor, so a test
        can observe the EXACT moment the object is actually freed. */
    class DeathWatchBuffer final : public SampleBuffer
    {
    public:
        DeathWatchBuffer (bool& destroyedFlag, int channels, int samples)
            : SampleBuffer (makeSilence (channels, samples), 44100.0, "deathwatch"),
              destroyed (destroyedFlag)
        {
            destroyed = false;
        }

        ~DeathWatchBuffer() override { destroyed = true; }

    private:
        static juce::AudioBuffer<float> makeSilence (int channels, int samples)
        {
            juce::AudioBuffer<float> audio (channels, samples);
            audio.clear();
            return audio;
        }

        bool& destroyed;
    };
}

class SampleBufferTest final : public juce::UnitTest
{
public:
    SampleBufferTest() : juce::UnitTest ("RollForge SampleBuffer", testCategory) {}

    void runTest() override
    {
        beginTest ("SampleBuffer exposes its immutable audio");
        {
            auto buf = makeTestBuffer (2, 8);
            expectEquals (buf->getNumChannels(), 2);
            expectEquals (buf->getNumSamples(), 8);
            expectWithinAbsoluteError (buf->getSampleRate(), 48000.0, 1.0e-9);
            expect (buf->getName() == "test");

            expectWithinAbsoluteError (buf->getSample (1, 3), 103.0f, 1.0e-6f);
            // Channel is clamped, so a too-high channel reads the last one.
            expectWithinAbsoluteError (buf->getSample (5, 3), buf->getSample (1, 3), 1.0e-6f);
            // Out-of-range sample indices read as silence, never out of bounds.
            expectWithinAbsoluteError (buf->getSample (0, 999), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (buf->getSample (0, -1),  0.0f, 1.0e-6f);
        }

        beginTest ("retirement pool frees a buffer only after the last voice releases it");
        {
            SampleRetirementPool pool;

            auto padRef = makeTestBuffer();          // refcount 1 (the pad holds it)
            SampleBuffer::Ptr voiceRef = padRef;     // refcount 2 (a voice is playing it)
            expectEquals (padRef->getReferenceCount(), 2);

            pool.retire (padRef);                    // pool takes a ref -> 3
            padRef = nullptr;                        // pad drops its ref -> 2 (pool + voice)
            expectEquals (pool.getNumRetired(), 1);
            expectEquals (voiceRef->getReferenceCount(), 2);

            // Voice still active: a sweep must NOT reclaim the buffer.
            expectEquals (pool.sweep(), 0);
            expectEquals (pool.getNumRetired(), 1);

            // Voice finishes -> drops its ref (2 -> 1). Only the pool holds it
            // now, so this decref cannot trigger a delete — the RT guarantee.
            SampleBuffer* raw = voiceRef.get();
            voiceRef = nullptr;
            expectEquals (raw->getReferenceCount(), 1);   // still alive, pool owns it

            // Now a message-thread sweep reclaims it.
            expectEquals (pool.sweep(), 1);
            expectEquals (pool.getNumRetired(), 0);
            // `raw` is dangling from here on — deliberately not dereferenced again.
        }

        beginTest ("retired buffer is destroyed on the sweep that reclaims it");
        {
            bool destroyed = false;
            SampleRetirementPool pool;
            SampleBuffer::Ptr voiceRef;

            {
                SampleBuffer::Ptr padRef = new DeathWatchBuffer (destroyed, 1, 4);
                voiceRef = padRef;      // a voice takes a ref
                pool.retire (padRef);   // pool takes a ref
            }                           // padRef leaves scope -> pad's ref dropped

            expect (! destroyed);       // pool + voice still hold it
            expectEquals (pool.sweep(), 0);
            expect (! destroyed);

            voiceRef = nullptr;         // voice releases -> only the pool holds it
            expect (! destroyed);       // the decref did not delete it
            expectEquals (pool.sweep(), 1);
            expect (destroyed);         // the sweep ran the destructor (message thread)
        }

        beginTest ("retire ignores null and duplicate buffers");
        {
            SampleRetirementPool pool;

            pool.retire (nullptr);
            expectEquals (pool.getNumRetired(), 0);

            auto buf = makeTestBuffer();
            pool.retire (buf);
            pool.retire (buf);          // duplicate -> ignored
            expectEquals (pool.getNumRetired(), 1);

            buf = nullptr;              // drop the local ref -> only the pool holds it
            expectEquals (pool.sweep(), 1);
            expectEquals (pool.getNumRetired(), 0);
        }
    }
};

static SampleBufferTest sampleBufferTest;

} // namespace rollforge::tests
