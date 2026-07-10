// RollForge — Categoriser + FeatureExtractor tests (Phase 5, commit 1).
//
// Headless: filename tokens categorise named samples (>= 85%), and the audio-
// feature fallback classifies un-named bright/dark samples sensibly.

#include "library/Categoriser.h"
#include "library/FeatureExtractor.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class CategoriserTest final : public juce::UnitTest
{
public:
    CategoriserTest() : juce::UnitTest ("RollForge Categoriser", testCategory) {}

    void runTest() override
    {
        beginTest ("filename tokens categorise named samples (>= 85%)");
        {
            struct Case { const char* name; SoundCategory expected; };
            const Case cases[] = {
                { "Kick_808.wav",       SoundCategory::Kick },
                { "BD_deep.wav",        SoundCategory::Kick },
                { "Snare_Acoustic.wav", SoundCategory::Snare },
                { "SD_tight.wav",       SoundCategory::Snare },
                { "Clap_909.wav",       SoundCategory::Clap },
                { "HandClap.wav",       SoundCategory::Clap },
                { "ClosedHat.wav",      SoundCategory::HatClosed },
                { "HiHat_01.wav",       SoundCategory::HatClosed },
                { "OpenHat.wav",        SoundCategory::HatOpen },
                { "OHat_long.wav",      SoundCategory::HatOpen },
                { "Tom_Low.wav",        SoundCategory::Tom },
                { "FloorTom.wav",       SoundCategory::Tom },
                { "Shaker.wav",         SoundCategory::Perc },
                { "Crash_Cymbal.wav",   SoundCategory::Perc },
                { "Riser_FX.wav",       SoundCategory::Fx },
                { "Vinyl_Noise.wav",    SoundCategory::Fx },

                // Word boundaries: a token must not match a fragment of a longer word.
                { "Phat Kick.wav",      SoundCategory::Kick },
                { "Phatty_Kick.wav",    SoundCategory::Kick },
                { "That_Snare.wav",     SoundCategory::Snare },

                // ...while the splits that keep boundary matching useful still hold.
                { "KickDrum.wav",       SoundCategory::Kick },   // camelCase hump
                { "kick01.wav",         SoundCategory::Kick },   // letter -> digit
                { "808kick.wav",        SoundCategory::Kick },   // digit -> letter
                { "TR808_BD.wav",       SoundCategory::Kick },
                { "hihat.wav",          SoundCategory::HatClosed },
                { "HH_01.wav",          SoundCategory::HatClosed },
                { "Hi-Hat_Closed.wav",  SoundCategory::HatClosed },
                { "Open_Hat_02.wav",    SoundCategory::HatOpen },
                { "hatopen.wav",        SoundCategory::HatOpen },
                { "RimShot.wav",        SoundCategory::Snare },
                { "Low Tom 3.wav",      SoundCategory::Tom },
                { "Cowbell.wav",        SoundCategory::Perc },
                { "Wood_Block.wav",     SoundCategory::Perc },
                { "Uplifter_01.wav",    SoundCategory::Fx },
            };

            int correct = 0;
            const int total = (int) (sizeof (cases) / sizeof (cases[0]));
            for (const auto& c : cases)
            {
                const auto got = Categoriser::categorise (c.name, {});
                if (got == c.expected)
                    ++correct;
                else
                    logMessage (juce::String (c.name) + " -> " + categoryName (got)
                                + " (expected " + categoryName (c.expected) + ")");
            }
            expect ((float) correct / (float) total >= 0.85f);
        }

        beginTest ("filename tokens match whole words, not substrings");
        {
            // The bug: "hat" was matched as a SUBSTRING, and tested before "kick", so the
            // "hat" inside "Phat" won the race and a kick was filed as a closed hat.
            expectEquals ((int) Categoriser::fromFilename ("Phat Kick.wav"),
                          (int) SoundCategory::Kick);
            expectEquals ((int) Categoriser::fromFilename ("Phatty_Kick.wav"),
                          (int) SoundCategory::Kick);
            expectEquals ((int) Categoriser::fromFilename ("That_Snare.wav"),
                          (int) SoundCategory::Snare);

            // "OpenHat" camel-splits to "open hat", which also contains the word "hat" --
            // so the open hat must still be claimed before the closed hat.
            expectEquals ((int) Categoriser::fromFilename ("OpenHat.wav"),
                          (int) SoundCategory::HatOpen);
            expectEquals ((int) Categoriser::fromFilename ("openhat.wav"),
                          (int) SoundCategory::HatOpen);
            expectEquals ((int) Categoriser::fromFilename ("ClosedHat.wav"),
                          (int) SoundCategory::HatClosed);

            // Splits that keep short tokens working without letting them run wild.
            expectEquals ((int) Categoriser::fromFilename ("808kick.wav"),
                          (int) SoundCategory::Kick);
            expectEquals ((int) Categoriser::fromFilename ("BD_deep.wav"),
                          (int) SoundCategory::Kick);
            expectEquals ((int) Categoriser::fromFilename ("Bdrum.wav"),
                          (int) SoundCategory::Unknown);   // "bd" is not a word here
            expectEquals ((int) Categoriser::fromFilename ("Shaker.wav"),
                          (int) SoundCategory::Perc);

            // The extension is not a token: a ".fx" file is not an FX hit.
            expectEquals ((int) Categoriser::fromFilename ("Snare.fx"),
                          (int) SoundCategory::Snare);

            // A name with no usable token is Unknown, and categorise() then asks the audio.
            expectEquals ((int) Categoriser::fromFilename ("sound01.wav"),
                          (int) SoundCategory::Unknown);
        }

        beginTest ("feature fallback classifies un-named samples");
        {
            const double sr = 44100.0;

            // Bright, short: near-Nyquist alternation with a fade -> high ZCR.
            std::vector<float> bright (400, 0.0f);
            for (int i = 0; i < 400; ++i)
                bright[(size_t) i] = (i % 2 == 0 ? 1.0f : -1.0f) * (1.0f - (float) i / 400.0f);
            const auto fb = FeatureExtractor::analyse (bright.data(), (int) bright.size(), sr);
            const auto catBright = Categoriser::categorise ("sound01.wav", fb);
            expect (catBright == SoundCategory::HatClosed || catBright == SoundCategory::HatOpen);

            // Dark, sustained: 60 Hz sine -> very low ZCR, long body.
            std::vector<float> dark (22050, 0.0f);
            for (int i = 0; i < (int) dark.size(); ++i)
                dark[(size_t) i] = 0.8f * std::sin (2.0f * 3.14159265f * 60.0f * (float) i / (float) sr);
            const auto fd = FeatureExtractor::analyse (dark.data(), (int) dark.size(), sr);
            const auto catDark = Categoriser::categorise ("sound02.wav", fd);
            expect (catDark == SoundCategory::Kick || catDark == SoundCategory::Tom);
        }
    }
};

static CategoriserTest categoriserTest;

} // namespace rollforge::tests
