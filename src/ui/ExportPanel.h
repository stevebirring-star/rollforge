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

    // The int is the loop count: how many times to repeat the whole pattern in the
    // export (1/2/4/8). Repeats capture the per-bar probability + humanise variation
    // the engine generates, so a longer render isn't just a copy-paste of bar 1.
    std::function<void (int loops)> onExportMidi;
    std::function<void (int loops)> onExportWav;
    std::function<void (int loops)> onExportStems;

private:
    int selectedLoops() const;

    juce::Label      title;
    juce::Label      loopsCaption;
    juce::ComboBox   loopsBox;
    juce::TextButton exportMidiButton  { "Export MIDI" };
    juce::TextButton exportWavButton   { "Export WAV (mix)" };
    juce::TextButton exportStemsButton { "Export Stems (per pad)" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPanel)
};

} // namespace rollforge
