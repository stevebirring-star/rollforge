// RollForge — round-robin + velocity-layer tests (Credibility tier).
//
// Headless: a pad holding several samples picks between them per hit. Each layer here is
// a short DC block at a distinct level, so the first rendered sample says which layer
// played, and the assertions read as "that hit sounded layer 2". The blocks are shorter
// than a render block, so consecutive hits never overlap.

#include "engine/DrumEngine.h"
#include "engine/OfflineRenderer.h"
#include "engine/SampleRetirementPool.h"
#include "library/KitInstaller.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    constexpr double sr        = 44100.0;
    constexpr int    blockSize = 64;
    constexpr float  centrePan = 0.70710678f;   // equal-power pan at centre

    /** A constant-valued buffer, shorter than one render block. */
    SampleBuffer::Ptr dcLayer (float value, int n = 16)
    {
        juce::AudioBuffer<float> audio (1, n);
        for (int i = 0; i < n; ++i)
            audio.setSample (0, i, value);
        return new SampleBuffer (std::move (audio), sr, "dc");
    }

    void installLayers (DrumEngine& engine, const SampleBuffer::Ptr* layers, int n, LayerMode mode)
    {
        engine.pushSetPadLayers (0, layers, n, mode, VoiceParameters {}, noChokeGroup);
        juce::AudioBuffer<float> drain (2, blockSize);
        drain.clear();
        engine.process (drain);   // drain the setPad command
    }

    /** Fires one hit and returns the LAYER VALUE that sounded (gain/pan divided out). */
    float hitLayerValue (DrumEngine& engine, float velocity, int sampleLock = -1)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();
        engine.triggerPadNow (0, velocity, 0.0f, sampleLock);
        engine.renderInto (buffer, 0, blockSize);
        return std::abs (buffer.getSample (0, 0)) / (centrePan * velocity);
    }
}

class LayerTest final : public juce::UnitTest
{
public:
    LayerTest() : juce::UnitTest ("RollForge Layers", testCategory) {}

    void runTest() override
    {
        beginTest ("round-robin cycles through a pad's layers, one per hit");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            const SampleBuffer::Ptr layers[] = { dcLayer (0.1f), dcLayer (0.2f), dcLayer (0.3f) };
            installLayers (engine, layers, 3, LayerMode::roundRobin);

            const float expected[] = { 0.1f, 0.2f, 0.3f, 0.1f, 0.2f, 0.3f, 0.1f };
            for (int i = 0; i < 7; ++i)
                expectWithinAbsoluteError (hitLayerValue (engine, 1.0f), expected[i], 0.005f,
                                           "hit " + juce::String (i) + " played the wrong layer");
        }

        beginTest ("a single-layer pad is unchanged (the old behaviour, exactly)");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            auto only = dcLayer (0.5f);
            engine.pushSetPad (0, only, VoiceParameters {}, noChokeGroup);
            juce::AudioBuffer<float> drain (2, blockSize); drain.clear(); engine.process (drain);

            for (int i = 0; i < 4; ++i)
                expectWithinAbsoluteError (hitLayerValue (engine, 1.0f), 0.5f, 0.005f);
        }

        beginTest ("velocity mode picks a layer from how hard the hit is");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            const SampleBuffer::Ptr layers[] = { dcLayer (0.1f), dcLayer (0.2f),
                                                 dcLayer (0.3f), dcLayer (0.4f) };
            installLayers (engine, layers, 4, LayerMode::velocity);

            struct Case { float velocity; float layerValue; };
            const Case cases[] = { { 0.10f, 0.1f }, { 0.30f, 0.2f }, { 0.60f, 0.3f },
                                   { 0.99f, 0.4f }, { 1.00f, 0.4f } };   // v == 1 clamps to the top

            for (const auto& c : cases)
                expectWithinAbsoluteError (hitLayerValue (engine, c.velocity), c.layerValue, 0.005f,
                                           "velocity " + juce::String (c.velocity) + " picked the wrong layer");

            // Repeating the SAME velocity must repeat the same layer — velocity mode is a
            // lookup, not a cycle. (Round-robin at this point would return 0.1, 0.2, ...)
            for (int i = 0; i < 3; ++i)
                expectWithinAbsoluteError (hitLayerValue (engine, 0.60f), 0.3f, 0.005f);
        }

        beginTest ("a Step's sampleLock pins one layer, overriding the pad's own choice");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            const SampleBuffer::Ptr layers[] = { dcLayer (0.1f), dcLayer (0.2f), dcLayer (0.3f) };
            installLayers (engine, layers, 3, LayerMode::roundRobin);

            for (int i = 0; i < 4; ++i)
                expectWithinAbsoluteError (hitLayerValue (engine, 1.0f, /*sampleLock*/ 2), 0.3f, 0.005f,
                                           "a locked step must not cycle");

            // A lock never advances the cursor, so the next free hit is still layer 0 ...
            expectWithinAbsoluteError (hitLayerValue (engine, 1.0f), 0.1f, 0.005f);
            // ... and an out-of-range lock falls back to the pad's choice rather than crashing.
            expectWithinAbsoluteError (hitLayerValue (engine, 1.0f, /*sampleLock*/ 99), 0.2f, 0.005f);
        }

        beginTest ("round-robin keeps exports reproducible (prepare resets the cursor)");
        {
            // The engine is REUSED between renders — WavExporter::exportStems drives one
            // engine through all 16 pads, and the app renders the mix on an engine it may
            // render again. Without the cursor reset in prepare(), the second render
            // starts the cycle wherever the first left it: two exports of the same beat
            // differ, and stems stop summing to the mix. Silently, and only for layered
            // pads. A fresh engine per render would hide this, so don't use one here.
            auto patternOf16 = []
            {
                Pattern p;
                p.numLanes = 1; p.bpm = 120.0;
                p.lane (0).targetPad = 0; p.lane (0).length = 16;
                for (int s = 0; s < 16; ++s)   // 16 hits vs a 3-cycle: never a whole number
                    p.lane (0).step (s).on = true;
                return p;
            };

            auto maxDiff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
            {
                float d = 0.0f;
                const int n = juce::jmin (a.getNumSamples(), b.getNumSamples());
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        d = juce::jmax (d, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
                return d;
            };

            // No tail: the render must stop after exactly 16 triggers. With a tail the
            // clock fires two extra steps, 18 % 3 == 0, and the cursor lands back on
            // layer 0 by coincidence — the test would pass with the reset deleted.
            OfflineRenderer::Options opts;
            opts.applyMasterFx = false; opts.bars = 1; opts.tailSeconds = 0.0;
            const Pattern p = patternOf16();

            DrumEngine engine;
            Kit kit;
            SampleRetirementPool pool;
            const SampleBuffer::Ptr layers[] = { dcLayer (0.1f, 512), dcLayer (0.2f, 512),
                                                 dcLayer (0.3f, 512) };
            installLayersIntoPad (pool, kit, engine, 0, layers, 3);

            juce::AudioBuffer<float> first, second;
            OfflineRenderer::render (engine, p, first, opts);
            OfflineRenderer::render (engine, p, second, opts);   // same engine, second time

            expectEquals (first.getNumSamples(), second.getNumSamples());
            expect (first.getMagnitude (0, 0, first.getNumSamples()) > 0.0f);
            expectWithinAbsoluteError (maxDiff (first, second), 0.0f, 1.0e-7f,
                                       "two renders of the same round-robin pattern must be identical");

            // And the layers must actually alternate, or the feature does nothing.
            DrumEngine flatEngine;
            Kit flatKit;
            SampleRetirementPool flatPool;
            installSampleIntoPad (flatPool, flatKit, flatEngine, 0, layers[0]);
            juce::AudioBuffer<float> flat;
            OfflineRenderer::render (flatEngine, p, flat, opts);
            expect (maxDiff (first, flat) > 0.01f, "round-robin must vary the render");
        }

        beginTest ("installLayersIntoPad retires every outgoing layer");
        {
            DrumEngine engine;
            engine.prepare (sr, 512);
            SampleRetirementPool pool;
            Kit kit;

            const SampleBuffer::Ptr first[] = { dcLayer (0.1f), dcLayer (0.2f), dcLayer (0.3f) };
            installLayersIntoPad (pool, kit, engine, 0, first, 3);
            expectEquals (pool.getNumRetired(), 0, "the pad was empty; nothing to retire");
            expectEquals (kit.pad (0).numAlternates(), 3);

            const SampleBuffer::Ptr second[] = { dcLayer (0.4f) };
            installLayersIntoPad (pool, kit, engine, 0, second, 1);
            expectEquals (pool.getNumRetired(), 3, "all three old layers must be retired");
            expectEquals (kit.pad (0).numAlternates(), 1, "a one-layer install clears the stack");
        }
    }
};

static LayerTest layerTest;

} // namespace rollforge::tests
