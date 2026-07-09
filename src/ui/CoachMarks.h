#pragma once

// RollForge — CoachMarks: the first-run tour.
//
// An overlay that dims the window, cuts a hole around one control, and puts a short note
// beside it. Next, Skip, Escape. That is the whole of it.
//
// THE HOLE IS A REAL HOLE. hitTest() returns false inside it, so the control underneath is
// live: when the tour says "press this", you press it. A tour that highlights a button and
// then swallows the click is a tour that teaches you the app is broken.
//
// Every step is skippable, Escape ends it, and it never comes back on its own. It can be
// re-run from Help, which is the only reason it is allowed to exist at all: a tour you cannot
// summon when you finally have the question it answers is a tour shown at the wrong moment.
//
// The bubble is never placed over the hole, because a Component's children are not hit-tested
// when its own hitTest() says no, and the Next button would go dead.
//
// UI only. The owner supplies the steps and the components they point at.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace rollforge
{

class CoachMarks final : public juce::Component
{
public:
    struct Step
    {
        juce::Component* target = nullptr;   // what to cut a hole around; null = centre the bubble
        juce::String     title;
        juce::String     body;
    };

    CoachMarks() = default;

    /** Replaces the tour and shows step 0. Steps whose target is null are centred. */
    void setSteps (std::vector<Step> steps);

    /** Fired when the tour is finished or skipped. The owner deletes this component. */
    std::function<void()> onFinished;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void goToStep (int index);
    void finish();

    /** The target's bounds in our coordinates, padded. Empty when the step has no target. */
    juce::Rectangle<int> holeBounds() const;

    /** Where the bubble goes: beside the hole, never over it, always on screen. */
    juce::Rectangle<int> bubbleBounds() const;

    std::vector<Step> steps;
    int current = 0;

    juce::TextButton nextButton { "Next" };
    juce::TextButton skipButton { "Skip tour" };

    // Height is not a guess: 28 of padding, 13 for "2 of 6", 21 for the title and 34 for the
    // button row leave 82 for the body -- five lines at 12.5 px. A shorter bubble silently
    // clipped the copy, because drawFittedText shrinks rather than complains.
    static constexpr int bubbleWidth  = 336;
    static constexpr int bubbleHeight = 178;
    static constexpr int bodyLines    = 5;
    static constexpr int holePadding  = 6;
    static constexpr int gap          = 14;   // between hole and bubble

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CoachMarks)
};

} // namespace rollforge
