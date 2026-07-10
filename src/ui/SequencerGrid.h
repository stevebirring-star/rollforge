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
#include <utility>
#include <vector>

namespace rollforge
{

class SequencerGrid final : public juce::Component,
                            private juce::Timer
{
public:
    SequencerGrid (int numLanes, int numSteps);

    void setLaneLabel (int lane, const juce::String& text);
    void setLaneLocked (int lane, bool locked);                   // reflect lock state -> UI
    void setStep (int lane, int step, bool on, float velocity);   // reflect model -> UI

    /** How many of this lane's cells exist. Cells past it are drawn faint and unclickable
        (a 1/8-triplet lane uses 12 of the 16 columns). */
    void setLaneLength (int lane, int length);

    /** Reflect the lane's triplet flag into its "3" toggle (does not fire the callback). */
    void setLaneTriplet (int lane, bool triplet);

    /** The colour of the sound this lane fires. Tints its label and all of its steps. */
    void setLaneColour (int lane, juce::Colour colour);

    /** The step each lane is currently sounding; -1 for none. A triplet lane runs at its
        own rate, so the playhead is per lane rather than one column down the grid. */
    void setLanePlayhead (int lane, int step);
    void clearPlayheads();

    /** Briefly ring the given (lane, step) cells in amber — the "here's what Vary
        changed" teaching hook. Auto-clears after a short delay. */
    void flashChanged (const std::vector<std::pair<int, int>>& changedCells);

    std::function<void (int lane, int step, bool on, float velocity)> onStepEdit;
    std::function<void (int lane)> onLaneLockToggled;    // fired when a lane's padlock is clicked
    std::function<void (int lane, bool triplet)> onLaneTripletToggled;
    std::function<void()> onGestureStart;   // fired when a cell gesture begins

    void resized() override;

    int getNumLanes() const noexcept { return numLanes; }
    int getNumSteps() const noexcept { return numSteps; }

    static constexpr int labelColumnWidth = 108;  // padlock + triplet toggle + the lane's name

private:
    StepComponent* cell (int lane, int step) noexcept;
    void timerCallback() override;   // clears the Vary flash

    const int numLanes;
    const int numSteps;
    std::vector<int> lanePlayhead;   // per lane, -1 = none
    std::vector<int> laneLength;     // per lane, cells beyond it are inactive

    std::vector<std::pair<int, int>> flashed;   // cells currently ringed by flashChanged

    juce::OwnedArray<juce::Label>         laneLabels;
    juce::OwnedArray<LaneLockButton>      lockButtons;   // one per lane, left of the label
    juce::OwnedArray<juce::TextButton>    tripletButtons; // one per lane, a "3" toggle
    juce::OwnedArray<StepComponent>       cells;   // row-major: lane * numSteps + step

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerGrid)
};

} // namespace rollforge
