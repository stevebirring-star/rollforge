#pragma once

#include "engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

/** Phase 0 main view.

    Deliberately minimal: a title, a big "Play Blip" button that exercises the
    audio path, an "Audio Settings" button that opens the device selector, and
    a status line. Later phases replace the body with pads + sequencer while the
    engine ownership model stays the same. */
class MainComponent final : public juce::Component,
                            private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void openAudioSettings();
    void refreshStatus();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    AudioEngine engine;

    juce::Label      titleLabel;
    juce::Label      statusLabel;
    juce::TextButton blipButton    { "Play Blip" };
    juce::TextButton settingsButton { "Audio Settings" };

    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rollforge
