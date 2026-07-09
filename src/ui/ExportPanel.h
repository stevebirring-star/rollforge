#pragma once

// RollForge — ExportPanel: a small dialog with the export actions (MIDI, WAV mix,
// WAV stems) plus two chips you can drag straight into a DAW. The owner wires each
// button to a file-chooser + exporter, and each chip to a temp-file renderer. UI only.

#include "ui/DragExportButton.h"

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

    // Render the drag payload to a temp file and return it. Same loop count as above.
    std::function<juce::File (int loops)> onDragOutMidi;
    std::function<juce::File (int loops)> onDragOutWav;

private:
    int selectedLoops() const;

    juce::Label      title;
    juce::Label      loopsCaption;
    juce::ComboBox   loopsBox;
    juce::TextButton exportMidiButton  { "Export MIDI" };
    juce::TextButton exportWavButton   { "Export WAV (mix)" };
    juce::TextButton exportStemsButton { "Export Stems (per pad)" };
    juce::Label      dragCaption;
    DragExportButton dragMidiChip { "Drag MIDI", "Drag this into your DAW to drop the pattern as a MIDI clip" };
    DragExportButton dragWavChip  { "Drag Audio", "Drag this into your DAW to drop the rendered loop as audio" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPanel)
};

} // namespace rollforge
