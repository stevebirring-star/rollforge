#pragma once

// RollForge — ExportPanel: a small dialog with the export actions (MIDI, WAV mix,
// WAV stems). The owner wires each button to a file-chooser + exporter. UI only.
// (Project save/load UI + drag-out are follow-ups.)

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class ExportPanel final : public juce::Component
{
public:
    ExportPanel();

    void resized() override;

    std::function<void()> onExportMidi;
    std::function<void()> onExportWav;
    std::function<void()> onExportStems;

private:
    juce::Label      title;
    juce::TextButton exportMidiButton  { "Export MIDI" };
    juce::TextButton exportWavButton   { "Export WAV (mix)" };
    juce::TextButton exportStemsButton { "Export Stems (per pad)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPanel)
};

} // namespace rollforge
