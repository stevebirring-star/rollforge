#include "ui/MainComponent.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace rollforge
{

namespace colours
{
    static const juce::Colour background { 0xff1a1a1e };
    static const juce::Colour panel      { 0xff26262c };
    static const juce::Colour accent      { 0xff4cc2ff };
    static const juce::Colour text        { 0xffe8e8ec };
    static const juce::Colour textDim     { 0xff9a9aa4 };
}

MainComponent::MainComponent()
{
    setWantsKeyboardFocus (true);

    titleLabel.setText ("RollForge", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    statusLabel.setFont (juce::FontOptions (14.0f));
    statusLabel.setColour (juce::Label::textColourId, colours::textDim);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    blipButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    blipButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    blipButton.onClick = [this] { engine.triggerBlip(); };
    addAndMakeVisible (blipButton);

    settingsButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    settingsButton.setColour (juce::TextButton::textColourOffId, colours::text);
    settingsButton.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible (settingsButton);

    engine.initialise();
    engine.getDeviceManager().addChangeListener (this);
    refreshStatus();

    setSize (720, 420);
}

MainComponent::~MainComponent()
{
    // Stop listening before any teardown so no callback lands on a half-dead component.
    engine.getDeviceManager().removeChangeListener (this);

    if (settingsWindow != nullptr)
        settingsWindow.deleteAndZero();

    engine.shutdown();
}

void MainComponent::refreshStatus()
{
    if (auto* device = engine.getDeviceManager().getCurrentAudioDevice())
    {
        statusLabel.setText ("Audio: " + device->getName()
                                 + "  •  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz"
                                 + "  •  " + juce::String (device->getCurrentBufferSizeSamples()) + " samples",
                             juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("No audio device open — open Audio Settings to choose one.",
                             juce::dontSendNotification);
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    // AudioDeviceManager broadcasts whenever the device / sample rate / buffer
    // size changes (e.g. from the settings dialog); keep the status line in sync.
    if (source == &engine.getDeviceManager())
        refreshStatus();
}

void MainComponent::openAudioSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        engine.getDeviceManager(),
        /*minInputChannels*/  0, /*maxInputChannels*/  0,
        /*minOutputChannels*/ 1, /*maxOutputChannels*/ 2,
        /*showMidiInput*/     false,
        /*showMidiOutput*/    false,
        /*showChannelsAsStereoPairs*/ true,
        /*hideAdvancedOptionsWithButton*/ false);
    selector->setSize (500, 420);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector.release());
    options.dialogTitle              = "Audio Settings";
    options.dialogBackgroundColour   = colours::background;
    options.componentToCentreAround  = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar        = true;
    options.resizable                = true;

    // launchAsync() returns a self-deleting modeless window; the SafePointer
    // auto-nulls if the user closes it, and we tear it down in the destructor
    // if it is still open. The status line updates itself via
    // changeListenerCallback() (AudioDeviceManager is a ChangeBroadcaster),
    // so no exit callback is needed here.
    settingsWindow = options.launchAsync();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("Phase 0 — skeleton. Press the button (or the space bar) to hear a blip.",
                getLocalBounds().reduced (28).removeFromBottom (28),
                juce::Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (28);

    titleLabel.setBounds (area.removeFromTop (48));
    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (24);

    auto row = area.removeFromTop (56);
    blipButton.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (16);
    settingsButton.setBounds (row.removeFromLeft (200));
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        engine.triggerBlip();
        return true;
    }

    return false;
}

} // namespace rollforge
