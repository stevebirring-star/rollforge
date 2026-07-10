#include "ui/ExportPanel.h"

#include "ui/Theme.h"

namespace rollforge
{

ExportPanel::ExportPanel()
{
    title.setText ("EXPORT THE CURRENT PATTERN", juce::dontSendNotification);
    title.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme().textDim);
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    loopsCaption.setText ("Length", juce::dontSendNotification);
    loopsCaption.setColour (juce::Label::textColourId, theme().textDim);
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

    dragCaption.setText ("OR DRAG STRAIGHT INTO YOUR DAW", juce::dontSendNotification);
    dragCaption.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    dragCaption.setColour (juce::Label::textColourId, theme().textDim);
    dragCaption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (dragCaption);

    dragMidiChip.renderFile = [this] { return onDragOutMidi ? onDragOutMidi (selectedLoops()) : juce::File(); };
    dragWavChip.renderFile  = [this] { return onDragOutWav  ? onDragOutWav  (selectedLoops()) : juce::File(); };
    addAndMakeVisible (dragMidiChip);
    addAndMakeVisible (dragWavChip);

    setSize (320, 300);
}

int ExportPanel::selectedLoops() const
{
    const int id = loopsBox.getSelectedId();
    return id > 0 ? id : 1;
}

void ExportPanel::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.fillAll (t.background);

    // A rule between "save it somewhere" and "drag it into your DAW": two different jobs.
    const float y = (float) dragCaption.getY() - 7.0f;
    g.setColour (t.hairline);
    g.drawLine (12.0f, y, (float) getWidth() - 12.0f, y, 1.0f);
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

    r.removeFromTop (12);
    dragCaption.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);
    auto dragRow = r.removeFromTop (30);
    dragMidiChip.setBounds (dragRow.removeFromLeft (dragRow.getWidth() / 2 - 4));
    dragRow.removeFromLeft (8);
    dragWavChip.setBounds (dragRow);
}

} // namespace rollforge
