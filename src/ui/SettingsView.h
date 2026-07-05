#pragma once

// RollForge — SettingsView: the app preferences dialog. Hosts the audio device
// selector (device + buffer size) and adds a UI-scale chooser + the library's
// sample folders, persisted via AppSettings. UI only.

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

    void resized() override;

    /** Fired when the UI-scale changes (already persisted). */
    std::function<void (float)> onScaleChanged;

private:
    void addSampleFolder();
    void updateFoldersLabel();

    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    juce::Label      scaleLabel;
    juce::ComboBox   scaleBox;
    juce::Label      foldersLabel;
    juce::TextButton addFolderButton { "Add sample folder..." };

    AppSettings settings;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsView)
};

} // namespace rollforge
