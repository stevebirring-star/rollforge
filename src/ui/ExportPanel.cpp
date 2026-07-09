#include "ui/ExportPanel.h"

namespace rollforge
{

ExportPanel::ExportPanel()
{
    title.setText ("Export the current pattern", juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, juce::Colour (0xffe8e8ec));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    loopsCaption.setText ("Length", juce::dontSendNotification);
    loopsCaption.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
    loopsCaption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (loopsCaption);

    // Item id == the loop count, so selectedLoops() reads it directly.
    loopsBox.addItem ("1 loop",  1);
    loopsBox.addItem ("2 loops", 2);
    loopsBox.addItem ("4 loops", 4);
    loopsBox.addItem ("8 loops", 8);
    loopsBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (loopsBox);

    exportMidiButton.onClick  = [this] { if (onExportMidi)  onExportMidi  (selectedLoops()); };
    exportWavButton.onClick   = [this] { if (onExportWav)   onExportWav   (selectedLoops()); };
    exportStemsButton.onClick = [this] { if (onExportStems) onExportStems (selectedLoops()); };
    addAndMakeVisible (exportMidiButton);
    addAndMakeVisible (exportWavButton);
    addAndMakeVisible (exportStemsButton);

    setSize (320, 232);
}

int ExportPanel::selectedLoops() const
{
    const int id = loopsBox.getSelectedId();
    return id > 0 ? id : 1;
}

void ExportPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    title.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);

    auto loopsRow = r.removeFromTop (26);
    loopsCaption.setBounds (loopsRow.removeFromLeft (54));
    loopsRow.removeFromLeft (8);
    loopsBox.setBounds (loopsRow.removeFromLeft (140));
    r.removeFromTop (10);

    exportMidiButton.setBounds (r.removeFromTop (34));
    r.removeFromTop (8);
    exportWavButton.setBounds (r.removeFromTop (34));
    r.removeFromTop (8);
    exportStemsButton.setBounds (r.removeFromTop (34));
}

} // namespace rollforge
