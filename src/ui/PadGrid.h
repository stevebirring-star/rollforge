#pragma once

// RollForge — PadGrid: the 4x4 grid of 16 PadComponents.
//
// Lays the pads out and forwards each pad's trigger / file-drop up to the app
// via callbacks. Pad index i sits at column i%4, row i/4 (row 0 = top), matching
// the model Kit's coordinate mapping.

#include "ui/PadComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace rollforge
{

class PadGrid final : public juce::Component
{
public:
    PadGrid();

    void setPadLabel (int index, const juce::String& text);
    juce::String getPadLabel (int index) const;
    void setPadLevel (int index, float level);                       // live meter (UI timer)
    void setPadWaveform (int index, const std::vector<float>& peaks); // sample thumbnail
    void flashPad (int index);
    void setPadMuted   (int index, bool muted);     // reflect mute state into the pad
    void setPadSoloed  (int index, bool soloed);    // reflect solo state into the pad
    void setPadAudible (int index, bool audible);   // dim a pad that won't sound
    void setPadReverse (int index, bool reversed);  // reflect reverse state into the pad
    void setPadTrim    (int index, float start, float end);   // reflect the trim region
    void setPadAccent  (int index, juce::Colour colour);      // the pad's sound colour

    std::function<void (int padIndex, float velocity)>        onPadTrigger;
    std::function<void (int padIndex)>                        onPadRelease;   // note-repeat hold end
    std::function<void (int padIndex, const juce::StringArray& files)> onPadFilesDropped;
    std::function<void (int padIndex, bool muted)>            onPadMute;
    std::function<void (int padIndex, bool soloed)>           onPadSolo;
    std::function<void (int padIndex, bool reversed)>         onPadReverse;
    std::function<void (int padIndex, float start, float end)> onPadTrim;
    std::function<void (int padIndex)>                        onPadInspect;   // right-click a pad

    /** The pad's on-screen bounds, so the owner can anchor a CallOutBox to it. */
    juce::Rectangle<int> getPadScreenBounds (int index) const;

    void resized() override;

private:
    static constexpr int numColumns = 4;
    static constexpr int numRows    = 4;
    static constexpr int numPads    = numColumns * numRows;

    juce::OwnedArray<PadComponent> pads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
};

} // namespace rollforge
