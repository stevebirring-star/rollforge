#pragma once

// RollForge — AboutView: the "Help & About" dialog content. Shows the app name +
// version and a scrollable quick-start covering the main features. UI only.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class AboutView final : public juce::Component
{
public:
    AboutView();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextEditor body;   // read-only, scrollable quick-start text

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutView)
};

} // namespace rollforge
