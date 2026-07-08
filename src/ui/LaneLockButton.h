#pragma once

// RollForge — LaneLockButton: a small padlock toggle in a sequencer lane header.
//
// Click toggles the lane's lock. A locked lane's steps are preserved when a new
// beat is generated (Make a Beat / Reroll) — "keep the kick, gamble the rest". It
// reflects state via setLocked() and reports clicks via onToggle. UI only.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class LaneLockButton final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    LaneLockButton();

    void setLocked (bool shouldBeLocked);          // reflects state; does not fire onToggle
    bool isLocked() const noexcept { return locked; }

    std::function<void()> onToggle;                // fired on click (the owner flips the state)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

private:
    bool locked = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneLockButton)
};

} // namespace rollforge
