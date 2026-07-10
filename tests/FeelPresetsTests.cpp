// RollForge — FeelPresets tests: every feel names + maps to sane humanise/swing.

#include "model/FeelPresets.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class FeelPresetsTest final : public juce::UnitTest
{
public:
    FeelPresetsTest() : juce::UnitTest ("RollForge FeelPresets", testCategory) {}

    void runTest() override
    {
        beginTest ("every feel has a name + in-range settings");
        {
            for (int i = 0; i < FeelPresets::NumFeels; ++i)
            {
                const auto f = (FeelPresets::Feel) i;
                expect (juce::String (FeelPresets::name (f)).isNotEmpty());
                const auto s = FeelPresets::settingsFor (f);
                expect (s.humanise >= 0.0f && s.humanise <= 1.0f);
                expect (s.swing    >= 0.0f && s.swing    <= 1.0f);
            }
        }

        beginTest ("Straight is dead-quantized; looser feels add movement");
        {
            const auto straight = FeelPresets::settingsFor (FeelPresets::Straight);
            expectEquals (straight.humanise, 0.0f);
            expectEquals (straight.swing,    0.0f);

            const auto drunk = FeelPresets::settingsFor (FeelPresets::Drunk);
            expect (drunk.humanise > FeelPresets::settingsFor (FeelPresets::TrapTight).humanise);

            const auto bb = FeelPresets::settingsFor (FeelPresets::BoomBapLoose);
            expect (bb.swing > FeelPresets::settingsFor (FeelPresets::TrapTight).swing);
        }
    }
};

static FeelPresetsTest feelPresetsTest;

} // namespace rollforge::tests
