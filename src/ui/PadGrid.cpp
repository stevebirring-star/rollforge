#include "ui/PadGrid.h"

namespace rollforge
{

PadGrid::PadGrid()
{
    for (int i = 0; i < numPads; ++i)
    {
        auto* pad = pads.add (new PadComponent (i));

        pad->onTrigger = [this] (int index, float velocity)
        {
            if (onPadTrigger)
                onPadTrigger (index, velocity);
        };
        pad->onFileDropped = [this] (int index, const juce::File& file)
        {
            if (onPadFileDropped)
                onPadFileDropped (index, file);
        };

        addAndMakeVisible (pad);
    }
}

void PadGrid::setPadLabel (int index, const juce::String& text)
{
    if (auto* pad = pads[index])
        pad->setLabelText (text);
}

void PadGrid::flashPad (int index)
{
    if (auto* pad = pads[index])
        pad->flash();
}

void PadGrid::resized()
{
    auto area = getLocalBounds();
    const int cellW = area.getWidth()  / numColumns;
    const int cellH = area.getHeight() / numRows;

    for (int i = 0; i < numPads; ++i)
        if (auto* pad = pads[i])
            pad->setBounds ((i % numColumns) * cellW, (i / numColumns) * cellH, cellW, cellH);
}

} // namespace rollforge
