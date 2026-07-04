#pragma once

// RollForge — PadComponent: one clickable drum pad.
//
// Click to audition (fires onTrigger); drop an audio file on it to load it into
// the pad (fires onFileDropped). Flashes briefly when triggered. UI only — it
// talks to the engine through the std::function callbacks the PadGrid wires up.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class PadComponent final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit PadComponent (int padIndex);

    void setLabelText (const juce::String& text);

    /** Brief visual flash (also used for keyboard / MIDI triggers). */
    void flash();

    std::function<void (int padIndex, float velocity)>       onTrigger;
    std::function<void (int padIndex, const juce::File& file)> onFileDropped;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDragAndDrop (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit  (const juce::StringArray& files) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;

    const int    index;
    juce::String label;
    float        flashLevel = 0.0f;
    bool         dragOver   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};

} // namespace rollforge
