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
    void setPadLevel (int index, float level);                       // live meter (UI timer)
    void setPadWaveform (int index, const std::vector<float>& peaks); // sample thumbnail
    void flashPad (int index);

    std::function<void (int padIndex, float velocity)>        onPadTrigger;
    std::function<void (int padIndex, const juce::File& file)> onPadFileDropped;

    void resized() override;

private:
    static constexpr int numColumns = 4;
    static constexpr int numRows    = 4;
    static constexpr int numPads    = numColumns * numRows;

    juce::OwnedArray<PadComponent> pads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
};

} // namespace rollforge
