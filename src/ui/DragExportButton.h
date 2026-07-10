#pragma once

// RollForge — DragExportButton: a chip you drag straight into a DAW.
//
// It renders the current pattern to a temp file and hands that file to the OS drag
// service, so dropping it on Ableton/Reaper/Bitwig creates a MIDI or audio clip.
//
// performExternalDragDropOfFiles() is a STATIC DragAndDropContainer method that only
// needs a source component with a window peer, so nothing here has to be a
// DragAndDropContainer, and the chip works from inside a modal dialog.
//
// The file is rendered on mouse-DOWN, not part-way through the drag: the Linux
// implementation grabs the pointer synchronously, and a receiver may read the path
// lazily, even after the drag ends. So the bytes must already be on disk before the
// drag starts, and must NOT be deleted when it finishes. Temp files are given stable
// names (each drag overwrites) and swept at the next app launch.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class DragExportButton final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    /** @param label   text drawn on the chip.
        @param tooltip hover text. */
    DragExportButton (juce::String label, juce::String tooltip);

    /** Renders the file to drag and returns it. Called on mouse-down, on the message
        thread. Return an empty File to cancel the drag (e.g. the render failed). */
    std::function<juce::File()> renderFile;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** <temp>/RollForge — where drag payloads are written. Created on demand. */
    static juce::File dragTempDirectory();

    /** Deletes anything left in dragTempDirectory() by earlier runs. Call once at
        startup: the files must outlive their drag, so nothing can clean them up then. */
    static void sweepStaleTempFiles();

private:
    static constexpr int dragThresholdPixels = 8;

    juce::String label;
    juce::File   pendingFile;      // rendered on mouse-down, handed to the OS on drag
    bool         dragStarted = false;
    bool         hovered     = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragExportButton)
};

} // namespace rollforge
