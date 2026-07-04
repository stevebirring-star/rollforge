#include "ui/ExportPanel.h"

namespace rollforge
{

ExportPanel::ExportPanel()
{
    title.setText ("Export the current pattern", juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, juce::Colour (0xffe8e8ec));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    exportMidiButton.onClick  = [this] { if (onExportMidi)  onExportMidi(); };
    exportWavButton.onClick   = [this] { if (onExportWav)   onExportWav(); };
    exportStemsButton.onClick = [this] { if (onExportStems) onExportStems(); };
    addAndMakeVisible (exportMidiButton);
    addAndMakeVisible (exportWavButton);
    addAndMakeVisible (exportStemsButton);

    setSize (320, 190);
}

void ExportPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    title.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    exportMidiButton.setBounds (r.removeFromTop (34));
    r.removeFromTop (8);
    exportWavButton.setBounds (r.removeFromTop (34));
    r.removeFromTop (8);
    exportStemsButton.setBounds (r.removeFromTop (34));
}

} // namespace rollforge
