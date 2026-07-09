#pragma once

// RollForge — SettingsView: the app preferences dialog. Hosts the audio device selector
// (device + buffer size) and a UI-scale chooser, persisted via AppSettings. UI only.
//
// It used to have an "Add folder" button too. Nothing ever read the folders it saved, so it
// showed a count that meant nothing and scanned nothing. Sample folders belong to the library,
// which is where you can also see them, watch them and stop watching them.

#include "app/AppSettings.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace rollforge
{

class SettingsView final : public juce::Component
{
public:
    explicit SettingsView (juce::AudioDeviceManager& deviceManager);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Fired when the UI-scale changes (already persisted). */
    std::function<void (float)> onScaleChanged;

private:

    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    juce::Label      scaleLabel;
    juce::ComboBox   scaleBox;

    AppSettings settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsView)
};

} // namespace rollforge
