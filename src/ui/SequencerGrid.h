#pragma once

// RollForge — SequencerGrid: a lanes x steps grid of StepComponents, one row per
// lane (labelled with its pad's sound) and a highlighted playhead column.
//
// It edits nothing itself; it reflects model state via setStep()/setLaneLabel()
// and reports each cell edit up through onStepEdit. UI only.

#include "ui/StepComponent.h"
#include "ui/LaneLockButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class SequencerGrid final : public juce::Component
{
public:
    SequencerGrid (int numLanes, int numSteps);

    void setLaneLabel (int lane, const juce::String& text);
    void setLaneLocked (int lane, bool locked);                   // reflect lock state -> UI
    void setStep (int lane, int step, bool on, float velocity);   // reflect model -> UI
    void setPlayheadStep (int step);                              // -1 = none

    std::function<void (int lane, int step, bool on, float velocity)> onStepEdit;
    std::function<void (int lane)> onLaneLockToggled;   // fired when a lane's padlock is clicked
    std::function<void()> onGestureStart;   // fired when a cell gesture begins

    void resized() override;

    int getNumLanes() const noexcept { return numLanes; }
    int getNumSteps() const noexcept { return numSteps; }

    static constexpr int labelColumnWidth = 90;   // width of the lane-label column

private:
    StepComponent* cell (int lane, int step) noexcept;

    const int numLanes;
    const int numSteps;
    int       playheadStep = -1;

    juce::OwnedArray<juce::Label>         laneLabels;
    juce::OwnedArray<LaneLockButton>      lockButtons;   // one per lane, left of the label
    juce::OwnedArray<StepComponent>       cells;   // row-major: lane * numSteps + step

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerGrid)
};

} // namespace rollforge
