#pragma once

// RollForge — AboutView: the "Help & About" dialog content. Shows the app name + version, a
// button that re-runs the guided tour, and a scrollable quick-start covering every user-facing
// feature. UI only.
//
// The tour lives behind a button here because a first-run tour is shown at the one moment the
// user has no questions. This is where they come back when they do.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class AboutView final : public juce::Component
{
public:
    AboutView();

    /** Fired by "Take the tour". The owner closes this dialog and starts the coach marks. */
    std::function<void()> onStartTour;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton tourButton { "Take the tour" };
    juce::TextEditor body;   // read-only, scrollable quick-start text

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutView)
};

} // namespace rollforge
