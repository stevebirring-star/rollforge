#pragma once

// RollForge — UndoableActions: juce::UndoManager-backed edits of a Pattern.
//
// SetStepAction records the before/after value of one step and, on perform/undo,
// writes it into the Pattern and calls back so the view + engine can refresh.
// The app owns the Pattern + UndoManager; the action holds a reference to that
// Pattern (kept alive for the app's lifetime).
//
// MODEL LAYER: depends on juce_data_structures (UndoableAction) — a non-GUI JUCE
// module — plus model/Pattern.h. No JUCE GUI includes.

#include "model/Pattern.h"

#include <juce_data_structures/juce_data_structures.h>

#include <functional>

namespace rollforge
{

class SetStepAction final : public juce::UndoableAction
{
public:
    SetStepAction (Pattern& pattern, int lane, int step,
                   Step before, Step after,
                   std::function<void (int lane, int step)> onApplied)
        : pattern (pattern), lane (lane), step (step),
          before (before), after (after), onApplied (std::move (onApplied))
    {
    }

    bool perform() override { apply (after);  return true; }
    bool undo()    override { apply (before); return true; }

    int getSizeInUnits() override { return (int) sizeof (*this); }

private:
    void apply (const Step& value)
    {
        if (lane >= 0 && lane < pattern.numLanes && step >= 0 && step < maxStepsPerLane)
        {
            pattern.lane (lane).step (step) = value;
            if (onApplied)
                onApplied (lane, step);
        }
    }

    Pattern& pattern;
    int      lane;
    int      step;
    Step     before;
    Step     after;
    std::function<void (int, int)> onApplied;
};

} // namespace rollforge
