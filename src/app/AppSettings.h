#pragma once

// RollForge — AppSettings: persisted user preferences, stored as JSON under the user app-data
// dir. Pure (juce_core) so the round-trip is headless-testable; the SettingsView edits it and
// the audio device config is handled separately by the AudioDeviceManager.

#include <juce_core/juce_core.h>

namespace rollforge
{

struct AppSettings
{
    float uiScale = 1.0f;      // 1.0 / 1.25 / 1.5

    // LEGACY, read-only. Settings once had an "Add folder" button that wrote here, and
    // nothing ever read it: the folders were never scanned. The watch list now lives in
    // library.db, owned by FolderWatcher. This field survives only so MainComponent can move
    // anyone's orphaned folders across on first launch, and is cleared once it has.
    juce::StringArray sampleFolders;

    juce::String toJson() const;
    static AppSettings fromJson (const juce::String& json);

    static juce::File settingsFile();       // <app-data>/RollForge/settings.json
    bool save() const;                      // -> settingsFile()
    static AppSettings load();              // <- settingsFile() (defaults if absent)

    bool saveTo (const juce::File& file) const;
    static AppSettings loadFrom (const juce::File& file);
};

} // namespace rollforge
