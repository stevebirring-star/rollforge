#pragma once

// RollForge — PadComponent: one clickable drum pad.
//
// Click to audition (fires onTrigger); drop an audio file on it to load it into
// the pad (fires onFileDropped). Flashes briefly when triggered. UI only — it
// talks to the engine through the std::function callbacks the PadGrid wires up.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace rollforge
{

class PadComponent final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit PadComponent (int padIndex);

    void setLabelText (const juce::String& text);

    /** Live output level 0..1 for the pad's meter. Call regularly (the UI timer):
        applies fast-attack / slow-release smoothing for a VU feel. */
    void setMeter (float level);

    /** Downsampled |amplitude| peaks (0..1) of the pad's sample, drawn as a
        thumbnail behind the label. Empty clears it. */
    void setWaveform (const std::vector<float>& peaks);

    /** Brief visual flash (also used for keyboard / MIDI triggers). */
    void flash();

    std::function<void (int padIndex, float velocity)>       onTrigger;
    std::function<void (int padIndex, const juce::File& file)> onFileDropped;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit  (const juce::StringArray& files) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;

    const int          index;
    juce::String       label;
    float              flashLevel  = 0.0f;
    float              meterLevel  = 0.0f;   // smoothed 0..1 for the level meter
    std::vector<float> waveform;             // downsampled |amp| peaks, 0..1
    bool               dragOver    = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};

} // namespace rollforge
