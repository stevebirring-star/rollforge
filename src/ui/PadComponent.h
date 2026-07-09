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
    const juce::String& getLabelText() const noexcept { return label; }

    /** Live output level 0..1 for the pad's meter. Call regularly (the UI timer):
        applies fast-attack / slow-release smoothing for a VU feel. */
    void setMeter (float level);

    /** Downsampled |amplitude| peaks (0..1) of the pad's sample, drawn as a
        thumbnail behind the label. Empty clears it. */
    void setWaveform (const std::vector<float>& peaks);

    /** Brief visual flash (also used for keyboard / MIDI triggers). */
    void flash();

    void setMuted   (bool muted);     // reflect state into the M button (no callback)
    void setSoloed  (bool soloed);    // reflect state into the S button (no callback)
    void setAudible (bool audible);   // dim the pad when it won't sound (mute/solo)
    void setReverse (bool reversed);  // reflect reverse state into the R button (no callback)
    void setTrim (float start, float end);   // set the sample-trim region [0..1] (no callback)

    std::function<void (int padIndex, float velocity)>       onTrigger;
    std::function<void (int padIndex)>                       onRelease;   // for note-repeat hold
    std::function<void (int padIndex, const juce::File& file)> onFileDropped;
    std::function<void (int padIndex, bool muted)>             onMute;
    std::function<void (int padIndex, bool soloed)>            onSolo;
    std::function<void (int padIndex, bool reversed)>          onReverse;
    std::function<void (int padIndex, float start, float end)> onTrim;
    std::function<void (int padIndex)>                         onInspect;   // right-click

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit  (const juce::StringArray& files) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void updateTrimFromX (float x);   // maps a drag x-position onto the active trim handle

    const int          index;
    juce::String       label;
    float              flashLevel  = 0.0f;
    float              meterLevel  = 0.0f;   // smoothed 0..1 for the level meter
    std::vector<float> waveform;             // downsampled |amp| peaks, 0..1
    bool               dragOver    = false;
    bool               audible     = true;   // false -> dimmed (muted, or excluded by a solo)
    float              trimStart   = 0.0f;   // sample-trim region [0..1]
    float              trimEnd     = 1.0f;
    bool               trimming    = false;  // dragging a trim handle in the bottom strip
    bool               draggingEnd = false;  // which handle (end vs start) is being dragged

    juce::TextButton   muteButton    { "M" };
    juce::TextButton   soloButton    { "S" };
    juce::TextButton   reverseButton { "R" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};

} // namespace rollforge
