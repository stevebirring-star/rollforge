#pragma once

// RollForge — MacroKnobs: the four Phase-4 master macro knobs (PUNCH / SPACE /
// CRUSH / DRIVE), each 0..1 driving the MasterBus directly. UI only.

#include "engine/MasterBus.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class MacroKnobs final : public juce::Component
{
public:
    explicit MacroKnobs (MasterBus& bus);

    /** Reflect the bus's current macro values back into the knobs (e.g. after a
        project load restores them on the bus). Does not re-drive the bus. */
    void syncFromBus();

    void resized() override;

private:
    MasterBus& bus;

    juce::Slider punchKnob, spaceKnob, crushKnob, driveKnob;
    juce::Label  punchLabel, spaceLabel, crushLabel, driveLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroKnobs)
};

} // namespace rollforge
