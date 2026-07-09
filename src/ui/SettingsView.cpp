#include "ui/SettingsView.h"

#include "ui/Theme.h"

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
    scaleLabel.setColour (juce::Label::textColourId, theme().textDim);
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



    setSize (520, 480);
}

void SettingsView::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.fillAll (t.background);

    // Separate the device (what the app talks to) from the app's own preferences.
    const float y = (float) scaleLabel.getY() - 8.0f;
    g.setColour (t.hairline);
    g.drawLine (8.0f, y, (float) getWidth() - 8.0f, y, 1.0f);
}

void SettingsView::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto scaleRow = r.removeFromBottom (28);
    scaleLabel.setBounds (scaleRow.removeFromLeft (70));
    scaleBox.setBounds (scaleRow.removeFromLeft (90));

    r.removeFromBottom (12);
    if (deviceSelector != nullptr)
        deviceSelector->setBounds (r);
}

} // namespace rollforge
