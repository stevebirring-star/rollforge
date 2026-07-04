#include "library/StarterKit.h"

#include "engine/SampleBuffer.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace rollforge
{
namespace StarterKit
{

namespace
{
    constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

    juce::AudioBuffer<float> makeBuffer (double sampleRate, double seconds)
    {
        juce::AudioBuffer<float> b (1, juce::jmax (1, (int) (sampleRate * seconds)));
        b.clear();
        return b;
    }

    // Kick: pitched sine with a fast pitch drop + click transient.
    SampleBuffer::Ptr synthKick (double sr, const char* name)
    {
        auto audio = makeBuffer (sr, 0.35);
        auto* d = audio.getWritePointer (0);
        double phase = 0.0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t     = (double) i / sr;
            const double freq  = 45.0 + 90.0 * std::exp (-t * 35.0);
            const double amp   = std::exp (-t * 7.0);
            const double click = std::exp (-t * 400.0) * 0.6;
            d[i] = (float) ((std::sin (phase) * amp + click * std::sin (phase * 4.0)) * 0.9);
            phase += kTwoPi * freq / sr;
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }

    // Tom: pitched sine, gentle pitch drop.
    SampleBuffer::Ptr synthTom (double sr, double baseFreq, const char* name)
    {
        auto audio = makeBuffer (sr, 0.30);
        auto* d = audio.getWritePointer (0);
        double phase = 0.0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t    = (double) i / sr;
            const double freq = baseFreq * (1.0 + 0.3 * std::exp (-t * 20.0));
            const double amp  = std::exp (-t * 6.0);
            d[i] = (float) (std::sin (phase) * amp * 0.8);
            phase += kTwoPi * freq / sr;
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }

    // Snare: decaying tone + noise body.
    SampleBuffer::Ptr synthSnare (double sr, const char* name)
    {
        auto audio = makeBuffer (sr, 0.20);
        auto* d = audio.getWritePointer (0);
        juce::Random rng (1234);
        double phase = 0.0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t     = (double) i / sr;
            const double tone  = std::sin (phase) * std::exp (-t * 30.0);
            const double noise = ((double) rng.nextFloat() * 2.0 - 1.0) * std::exp (-t * 18.0);
            d[i] = (float) ((0.5 * tone + 0.7 * noise) * 0.8);
            phase += kTwoPi * 190.0 / sr;
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }

    // Filtered noise burst (hats, cymbals, shaker, perc, rim). hpMix blends a
    // crude one-sample highpass (brightness); decayTau sets the length feel.
    SampleBuffer::Ptr synthNoise (double sr, double seconds, double decayTau,
                                  double hpMix, int seed, double gain, const char* name)
    {
        auto audio = makeBuffer (sr, seconds);
        auto* d = audio.getWritePointer (0);
        juce::Random rng ((juce::int64) seed);
        float prev = 0.0f;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t     = (double) i / sr;
            const float  white = rng.nextFloat() * 2.0f - 1.0f;
            const float  hp    = white - prev;
            prev = white;
            const float mixed = (float) ((1.0 - hpMix) * white + hpMix * hp);
            d[i] = (float) (mixed * std::exp (-t / decayTau) * gain);
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }

    // Clap: a few closely-spaced noise bursts with a short tail.
    SampleBuffer::Ptr synthClap (double sr, const char* name)
    {
        auto audio = makeBuffer (sr, 0.20);
        auto* d = audio.getWritePointer (0);
        juce::Random rng (555);
        const double bursts[4] = { 0.0, 0.010, 0.020, 0.030 };
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t = (double) i / sr;
            double env = std::exp (-t * 12.0) * 0.4;   // tail
            for (double bt : bursts)
                if (t >= bt)
                    env += std::exp (-(t - bt) * 220.0);

            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            d[i] = (float) (noise * juce::jmin (1.0, env) * 0.7);
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }

    // Cowbell: two detuned tones, quick decay.
    SampleBuffer::Ptr synthCowbell (double sr, const char* name)
    {
        auto audio = makeBuffer (sr, 0.25);
        auto* d = audio.getWritePointer (0);
        double p1 = 0.0, p2 = 0.0;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double t   = (double) i / sr;
            const double amp = std::exp (-t * 10.0);
            d[i] = (float) ((std::sin (p1) + 0.6 * std::sin (p2)) * amp * 0.5);
            p1 += kTwoPi * 540.0 / sr;
            p2 += kTwoPi * 800.0 / sr;
        }
        return new SampleBuffer (std::move (audio), sr, name);
    }
}

Kit build (double sampleRate)
{
    Kit kit;
    kit.name = "Starter Kit";

    kit.pad (Kick).alternates[0]      = synthKick   (sampleRate, "kick");
    kit.pad (Snare).alternates[0]     = synthSnare  (sampleRate, "snare");

    kit.pad (ClosedHat).alternates[0] = synthNoise  (sampleRate, 0.05, 0.012, 0.85, 11, 0.6f, "closed-hat");
    kit.pad (ClosedHat).chokeGroup    = hatChokeGroup;
    kit.pad (OpenHat).alternates[0]   = synthNoise  (sampleRate, 0.35, 0.10,  0.85, 12, 0.5f, "open-hat");
    kit.pad (OpenHat).chokeGroup      = hatChokeGroup;

    kit.pad (Clap).alternates[0]      = synthClap   (sampleRate, "clap");
    kit.pad (Rim).alternates[0]       = synthNoise  (sampleRate, 0.05, 0.008, 0.60, 13, 0.6f, "rim");
    kit.pad (LowTom).alternates[0]    = synthTom    (sampleRate, 110.0, "low-tom");
    kit.pad (MidTom).alternates[0]    = synthTom    (sampleRate, 160.0, "mid-tom");
    kit.pad (HighTom).alternates[0]   = synthTom    (sampleRate, 220.0, "high-tom");
    kit.pad (Crash).alternates[0]     = synthNoise  (sampleRate, 0.80, 0.35,  0.70, 14, 0.4f, "crash");
    kit.pad (Ride).alternates[0]      = synthNoise  (sampleRate, 0.50, 0.25,  0.55, 15, 0.4f, "ride");
    kit.pad (Cowbell).alternates[0]   = synthCowbell(sampleRate, "cowbell");
    kit.pad (Shaker).alternates[0]    = synthNoise  (sampleRate, 0.08, 0.02,  0.90, 16, 0.5f, "shaker");
    kit.pad (Perc1).alternates[0]     = synthTom    (sampleRate, 320.0, "perc1");
    kit.pad (Perc2).alternates[0]     = synthNoise  (sampleRate, 0.12, 0.03,  0.75, 17, 0.5f, "perc2");
    kit.pad (Kick2).alternates[0]     = synthKick   (sampleRate, "kick2");

    return kit;
}

} // namespace StarterKit
} // namespace rollforge
