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

/** Replaces a whole Pattern (used by "Make a Beat" / generation), remembering the
    prior pattern so one Cmd/Ctrl+Z reverts the entire generate. onApplied refreshes
    the view + engine after both perform and undo. */
class SetPatternAction final : public juce::UndoableAction
{
public:
    SetPatternAction (Pattern& target, Pattern before, Pattern after,
                      std::function<void()> onApplied)
        : target (target), before (std::move (before)), after (std::move (after)),
          onApplied (std::move (onApplied))
    {
    }

    bool perform() override { target = after;  if (onApplied) onApplied(); return true; }
    bool undo()    override { target = before; if (onApplied) onApplied(); return true; }

    // Two Patterns held by value — report it so the UndoManager prunes correctly.
    int getSizeInUnits() override { return (int) (sizeof (*this) + 2 * sizeof (Pattern)); }

private:
    Pattern& target;
    Pattern  before;
    Pattern  after;
    std::function<void()> onApplied;
};

} // namespace rollforge
