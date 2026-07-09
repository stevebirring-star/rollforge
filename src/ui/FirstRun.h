#pragma once

// RollForge — FirstRun: the tour's cover page, shown once (gated by a marker file in app-data,
// handled by the owner).
//
// It used to list five tips and then get out of the way. It now offers the guided tour
// instead, because a tip list and a tour that says the same things back to back is one
// overlay too many — and a tip you read before you have touched anything is a tip you have
// already forgotten. "No thanks" is a first-class answer.
//
// UI only.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class FirstRun final : public juce::Component
{
public:
    FirstRun();

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Take the tour. The owner marks it seen and starts the coach marks. */
    std::function<void()> onTakeTour;

    /** No thanks. The owner marks it seen and never asks again. */
    std::function<void()> onSkip;

private:
    juce::TextButton tourButton { "Show me around" };
    juce::TextButton skipButton { "No thanks" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirstRun)
};

} // namespace rollforge
