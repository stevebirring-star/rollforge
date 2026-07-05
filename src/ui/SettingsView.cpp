#include "ui/SettingsView.h"

namespace rollforge
{

SettingsView::SettingsView (juce::AudioDeviceManager& dm) : deviceManager (dm)
{
    settings = AppSettings::load();

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager,
        /*minInputChannels*/  0, /*maxInputChannels*/  0,
        /*minOutputChannels*/ 1, /*maxOutputChannels*/ 2,
        /*showMidiInput*/     false,
        /*showMidiOutput*/    false,
        /*showChannelsAsStereoPairs*/ true,
        /*hideAdvancedOptionsWithButton*/ false);
    addAndMakeVisible (*deviceSelector);

    scaleLabel.setText ("UI scale", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcc4));
    addAndMakeVisible (scaleLabel);

    scaleBox.addItem ("100%", 1);
    scaleBox.addItem ("125%", 2);
    scaleBox.addItem ("150%", 3);
    scaleBox.setSelectedId (settings.uiScale >= 1.5f ? 3 : (settings.uiScale >= 1.25f ? 2 : 1),
                            juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        const float scale = scaleBox.getSelectedId() == 3 ? 1.5f
                          : scaleBox.getSelectedId() == 2 ? 1.25f : 1.0f;
        settings.uiScale = scale;
        settings.save();
        if (onScaleChanged != nullptr)
            onScaleChanged (scale);
    };
    addAndMakeVisible (scaleBox);

    foldersLabel.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcc4));
    updateFoldersLabel();
    addAndMakeVisible (foldersLabel);

    addFolderButton.onClick = [this] { addSampleFolder(); };
    addAndMakeVisible (addFolderButton);

    setSize (520, 480);
}

void SettingsView::addSampleFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample folder");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (! dir.isDirectory())
                return;
            settings.sampleFolders.addIfNotAlreadyThere (dir.getFullPathName());
            settings.save();
            updateFoldersLabel();
        });
}

void SettingsView::updateFoldersLabel()
{
    foldersLabel.setText (juce::String (settings.sampleFolders.size()) + " sample folder(s) saved",
                          juce::dontSendNotification);
}

void SettingsView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto bottom = r.removeFromBottom (64);
    auto scaleRow = bottom.removeFromTop (28);
    scaleLabel.setBounds (scaleRow.removeFromLeft (70));
    scaleBox.setBounds (scaleRow.removeFromLeft (90));
    bottom.removeFromTop (6);
    addFolderButton.setBounds (bottom.removeFromLeft (170));
    bottom.removeFromLeft (10);
    foldersLabel.setBounds (bottom);

    r.removeFromBottom (8);
    if (deviceSelector != nullptr)
        deviceSelector->setBounds (r);
}

} // namespace rollforge
