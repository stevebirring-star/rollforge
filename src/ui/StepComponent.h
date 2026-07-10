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
    void setChanged (bool wasJustChanged);   // brief "Vary changed this" highlight

    /** False for a cell past the lane's length (a triplet lane uses 12 of 16). It is
        drawn faint and ignores the mouse — the step doesn't exist. */
    void setActive (bool isActive);

    /** The colour of the sound this lane triggers. A kick's steps are the kick's colour,
        everywhere. Brightness within the cell then carries velocity. */
    void setAccent (juce::Colour colour);

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
    bool  changed   = false;   // recently mutated by Vary -> amber ring, cleared by a timer
    bool  editing   = false;   // mouse held on this cell -> show a prominent % readout
    bool  downWasOn = false;    // step's on-state at mouse-down (click vs velocity-drag)
    bool  active    = true;     // false -> beyond the lane's length: faint, unclickable
    juce::Colour accent { 0xff4cc2ff };   // this lane's sound colour

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepComponent)
};

} // namespace rollforge
