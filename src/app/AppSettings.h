#pragma once

// RollForge — AppSettings: persisted user preferences (UI scale + sample folders),
// stored as JSON under the user app-data dir. Pure (juce_core) so the round-trip
// is headless-testable; the SettingsView edits it and the audio device config is
// handled separately by the AudioDeviceManager.

#include <juce_core/juce_core.h>

namespace rollforge
{

struct AppSettings
{
    float             uiScale = 1.0f;      // 1.0 / 1.25 / 1.5
    juce::StringArray sampleFolders;       // folders the library scans

    juce::String toJson() const;
    static AppSettings fromJson (const juce::String& json);

    static juce::File settingsFile();       // <app-data>/RollForge/settings.json
    bool save() const;                      // -> settingsFile()
    static AppSettings load();              // <- settingsFile() (defaults if absent)

    bool saveTo (const juce::File& file) const;
    static AppSettings loadFrom (const juce::File& file);
};

} // namespace rollforge
