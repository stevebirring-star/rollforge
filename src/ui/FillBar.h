#pragma once

// RollForge — FillBar: a control strip for the Phase-3 fill + feel features.
// Pick a style + intensity and hit FILL (or Reroll for a fresh variation) to
// generate a drum fill; the Humanise slider drives the sequencer's Robot<->Human
// feel directly. UI only — pattern generation is handled by the owner via onFill.

#include "engine/Sequencer.h"
#include "model/FillEngine.h"

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

private:
    void fire();

    Sequencer& sequencer;

    juce::ComboBox   styleBox;
    juce::Slider     intensitySlider;
    juce::TextButton fillButton   { "Make a Beat" };   // the flagship one-tap generate
    juce::TextButton rerollButton { "Reroll" };
    juce::Label      humaniseLabel;
    juce::Slider     humaniseSlider;

    std::uint64_t seed = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FillBar)
};

} // namespace rollforge
