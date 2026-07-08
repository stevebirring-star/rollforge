#pragma once

// RollForge — StepComponent: one step cell in the sequencer grid.
//
// Click toggles the step on/off (a click that turns it on also sets velocity from
// the click height); vertical drag on an on-cell sets velocity (top = loud). The
// cell shows on/off + velocity, and highlights when it is the current playhead
// step. Reports edits via onEdit. UI only.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class StepComponent final : public juce::Component
{
public:
    StepComponent() = default;

    /** Reflects model state into the cell (does not fire onEdit). */
    void setState (bool isOn, float vel);
    void setPlayhead (bool isCurrent);

    std::function<void (bool on, float velocity)> onEdit;
    std::function<void()> onGestureStart;   // fired at mouse-down (for undo transactions)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    float velocityForY (float y) const noexcept;

    bool  on        = false;
    float velocity  = 0.8f;
    bool  current   = false;
    bool  editing   = false;   // mouse held on this cell -> show a prominent % readout
    bool  downWasOn = false;    // step's on-state at mouse-down (click vs velocity-drag)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepComponent)
};

} // namespace rollforge
