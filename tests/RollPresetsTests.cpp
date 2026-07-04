// RollForge — RollPresets unit tests (Phase 3, commit 3).
//
// Headless: every preset names + compiles to hits, and the presets differ in
// character (density, fade, crescendo, pitch direction).

#include "model/RollCompiler.h"
#include "model/RollPresets.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    std::vector<RollEvent> hitsOf (RollPresets::Preset p)
    {
        return RollCompiler::compileRoll (RollPresets::make (p, 0.0, 2.0, 0));
    }
}

class RollPresetsTest final : public juce::UnitTest
{
public:
    RollPresetsTest() : juce::UnitTest ("RollForge RollPresets", testCategory) {}

    void runTest() override
    {
        beginTest ("every preset has a name and compiles to hits");
        {
            for (int i = 0; i < RollPresets::NumPresets; ++i)
            {
                const auto p = (RollPresets::Preset) i;
                expect (juce::String (RollPresets::name (p)).isNotEmpty());
                expect (hitsOf (p).size() > 0);
            }
        }

        beginTest ("presets differ in character");
        {
            // MachineGun is denser than Trap Triplet over the same span.
            expect (hitsOf (RollPresets::MachineGun).size() > hitsOf (RollPresets::TrapTriplet).size());

            const auto fade = hitsOf (RollPresets::FadeRoll);
            expect (fade.back().velocity < fade.front().velocity);        // fades out

            const auto cres = hitsOf (RollPresets::Crescendo);
            expect (cres.back().velocity > cres.front().velocity);        // swells in

            const auto rise = hitsOf (RollPresets::PitchRise);
            expect (rise.back().pitchSemitones > rise.front().pitchSemitones);   // pitch up

            const auto slide = hitsOf (RollPresets::DrillSlide);
            expect (slide.back().pitchSemitones < slide.front().pitchSemitones); // pitch down
        }
    }
};

static RollPresetsTest rollPresetsTest;

} // namespace rollforge::tests
