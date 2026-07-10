#include "ui/PadGrid.h"

#include "ui/GridGeometry.h"

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
        pad->onRelease = [this] (int index)
        {
            if (onPadRelease)
                onPadRelease (index);
        };
        pad->onFilesDropped = [this] (int index, const juce::StringArray& files)
        {
            if (onPadFilesDropped)
                onPadFilesDropped (index, files);
        };
        pad->onMute = [this] (int index, bool muted)  { if (onPadMute) onPadMute (index, muted); };
        pad->onSolo = [this] (int index, bool soloed) { if (onPadSolo) onPadSolo (index, soloed); };
        pad->onReverse = [this] (int index, bool rev) { if (onPadReverse) onPadReverse (index, rev); };
        pad->onTrim = [this] (int index, float s, float e) { if (onPadTrim) onPadTrim (index, s, e); };
        pad->onInspect = [this] (int index) { if (onPadInspect) onPadInspect (index); };

        addAndMakeVisible (pad);
    }
}

void PadGrid::setPadLabel (int index, const juce::String& text)
{
    if (auto* pad = pads[index])
        pad->setLabelText (text);
}

void PadGrid::setPadLevel (int index, float level)
{
    if (auto* pad = pads[index])
        pad->setMeter (level);
}

void PadGrid::setPadWaveform (int index, const std::vector<float>& peaks)
{
    if (auto* pad = pads[index])
        pad->setWaveform (peaks);
}

void PadGrid::flashPad (int index)
{
    if (auto* pad = pads[index])
        pad->flash();
}

void PadGrid::setPadMuted (int index, bool muted)
{
    if (auto* pad = pads[index])
        pad->setMuted (muted);
}

void PadGrid::setPadSoloed (int index, bool soloed)
{
    if (auto* pad = pads[index])
        pad->setSoloed (soloed);
}

void PadGrid::setPadAudible (int index, bool audible)
{
    if (auto* pad = pads[index])
        pad->setAudible (audible);
}

void PadGrid::setPadReverse (int index, bool reversed)
{
    if (auto* pad = pads[index])
        pad->setReverse (reversed);
}

void PadGrid::setPadTrim (int index, float start, float end)
{
    if (auto* pad = pads[index])
        pad->setTrim (start, end);
}

void PadGrid::resized()
{
    auto area = getLocalBounds();

    // Exact edges rather than a truncated cell width, so the 4x4 fills its space at any
    // window size and its right/bottom edges stay flush with the grids above it.
    for (int i = 0; i < numPads; ++i)
        if (auto* pad = pads[i])
            pad->setBounds (gridCell (i % numColumns, i / numColumns, numColumns, numRows, area));
}

void PadGrid::setPadAccent (int index, juce::Colour colour)
{
    if (auto* pad = pads[index])
        pad->setAccent (colour);
}

juce::String PadGrid::getPadLabel (int index) const
{
    if (index >= 0 && index < numPads)
        if (auto* pad = pads[index])
            return pad->getLabelText();
    return {};
}

juce::Rectangle<int> PadGrid::getPadScreenBounds (int index) const
{
    if (index < 0 || index >= numPads)
        return {};
    if (auto* pad = pads[index])
        return pad->getScreenBounds();
    return {};
}

} // namespace rollforge
