#pragma once

// RollForge — FirstRun: a one-time welcome overlay shown on the very first launch
// (gated by a marker file in app-data, handled by the owner). A translucent panel
// with a few tips + a dismiss button; never a modal tutorial. UI only.

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

    std::function<void()> onDismissed;

private:
    juce::TextButton startButton { "Let's go" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirstRun)
};

} // namespace rollforge
