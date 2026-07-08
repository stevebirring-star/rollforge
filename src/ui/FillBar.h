#pragma once

// RollForge — FillBar: a control strip for the Phase-3 fill + feel features.
// Pick a style + intensity and hit FILL (or Reroll for a fresh variation) to
// generate a drum fill; the Humanise slider drives the sequencer's Robot<->Human
// feel directly. UI only — pattern generation is handled by the owner via onFill.

#include "engine/Sequencer.h"
#include "model/FillEngine.h"
#include "model/FeelPresets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

namespace rollforge
{

class FillBar final : public juce::Component
{
public:
    explicit FillBar (Sequencer& sequencer);

    void resized() override;

    /** Fired when FILL or Reroll is pressed: (style, intensity 1..5, seed). */
    std::function<void (FillEngine::Style, int, std::uint64_t)> onFill;

    /** Fired when Vary is pressed: (intensity 1..5, seed). Mutates the CURRENT
        pattern instead of regenerating it. */
    std::function<void (int, std::uint64_t)> onVary;

    /** Fired when a Feel preset is picked: the new swing 0..1 (the owner reflects it
        onto the transport's swing control). Humanise is applied directly. */
    std::function<void (float)> onFeelSwing;

private:
    void fire();
    void fireVary();
    void applyFeel (FeelPresets::Feel feel);

    Sequencer& sequencer;

    juce::ComboBox   styleBox;
    juce::Slider     intensitySlider;
    juce::TextButton fillButton   { "Make a Beat" };   // the flagship one-tap generate
    juce::TextButton rerollButton { "Reroll" };
    juce::TextButton varyButton   { "Vary" };          // mutate the current beat, don't regenerate
    juce::Label      feelLabel;
    juce::ComboBox   feelBox;                          // named Humaniser+swing presets

    std::uint64_t seed = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FillBar)
};

} // namespace rollforge
