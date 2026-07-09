#pragma once

#include "engine/AudioEngine.h"
#include "engine/OfflineRenderer.h"
#include "engine/SampleRetirementPool.h"
#include "engine/WavExporter.h"
#include "model/MidiExporter.h"
#include "app/Autosave.h"
#include "app/AppSettings.h"
#include "app/FirstRunState.h"
#include "ui/SettingsView.h"
#include "ui/FirstRun.h"
#include "library/SampleLoader.h"
#include "library/StarterKit.h"
#include "library/KitInstaller.h"
#include "library/Slicer.h"
#include "library/LibraryDb.h"
#include "library/SimilarSearch.h"
#include "model/RollCompiler.h"
#include "model/RollPresets.h"
#include "model/PatternBank.h"
#include "model/UndoableActions.h"
#include "ui/PadGrid.h"
#include "ui/NoteRepeat.h"
#include "ui/TransportBar.h"
#include "ui/SequencerGrid.h"
#include "ui/FillBar.h"
#include "ui/PatternSlots.h"
#include "ui/RollBrushOverlay.h"
#include "ui/MacroKnobs.h"
#include "ui/BrowserPanel.h"
#include "ui/ExportPanel.h"
#include "ui/PadInspector.h"
#include "ui/BrandMark.h"
#include "ui/MasterMeter.h"
#include "ui/Theme.h"
#include "ui/AboutView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace rollforge
{

/** Phase 1 main view: a 4x4 pad grid (click to audition, drop a file to load),
    an Audio Settings button, and a status line. The engine ownership model is
    unchanged from Phase 0 — the UI talks to the engine only via its public API. */
class MainComponent final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void openSettings();
    void openLibrary();
    void openExport();
    void openHelp();
    void doExportMidi (int loops);
    void doExportWav (int loops);
    void doExportStems (int loops);
    void doSaveProject();
    void doOpenProject();
    void refreshStatus();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;                              // retirement sweep
    void loadFileIntoPad (int padIndex, const juce::File& file);
    void installKitSelection (const std::array<juce::String, kitNumPads>& paths);  // NEW KIT + New Sounds
    void rerollSounds (std::uint64_t seed);      // swap the kit's samples, keep the groove
    void selectSlot (int slot);                 // A..H: switch, quantised to the bar when playing
    void commitSlot (int slot);                 // the switch has landed: bring the UI across
    void refreshSlotStates();                   // which of A..H have anything in them

    /** Tempo and swing belong to the transport, not to a pattern slot; a Pattern only
        carries them so an export is self-contained. Stamping the live transport onto a
        pattern before it goes anywhere is what stops slot B exporting at the tempo it
        happened to be saved with. */
    void stampTransportOnto (Pattern& pattern);
    juce::String similarForPad (int padIndex);   // step this pad to its next-nearest library sound
    void rebuildSimilarSearch();                 // after a scan or a re-tag
    void loadLayersIntoPad (int padIndex, const juce::StringArray& files);   // round-robin layers
    void sliceLoopIntoPads (const juce::File& loop);   // chop a break across the pads at its onsets
    void auditionSample (const juce::File& file);      // play a browser sample on the preview pad
    void openPadInspector (int padIndex);              // right-click a pad: TONE + SEND
    void updatePadLabels();
    void updatePadWaveform (int padIndex);   // recompute a pad's waveform thumbnail from its sample
    void updateLaneLabelForPad (int padIndex);   // refresh sequencer lane label(s) targeting this pad
    void refreshPadAudibility();                 // reflect engine mute/solo into pad dimming
    void refreshCategoryColours();               // paint every pad + lane with its sound's colour
    void afterStepEdit (int lane, int step);   // reflect a step change into grid + engine
    void refreshGridFromPattern();             // re-reflect the whole editPattern into the grid
    RollRegion buildBrushRegion (int lane, int startStep, int lengthSteps, float density) const; // roll under the brush (paint + live meter share this)
    /** Renders the pattern to a temp file for a drag-out, returning it (empty on
        failure). MIDI when `asMidi`, otherwise a WAV mix of the master output. */
    juce::File renderDragFile (int loops, bool asMidi);

    /** Render settings for `bars` bars with the master strip exactly as it sounds.
        Every export goes through this — a caller that hand-rolled Options would
        silently drop whichever field it forgot (as the mix export did with EQ/comp). */
    OfflineRenderer::Options renderOptions (int bars, bool applyMasterFx = true);

    Project captureProject();                  // snapshot the session (pattern + FX)
    void    applyProject (const Project&);      // restore a session (pattern + FX)

    // Declared first -> destroyed last: the engine (holding pad sample refs)
    // outlives the Kit/pool/loader that also reference the samples.
    AudioEngine          engine;
    SampleLoader         loader;

    // The sample library. Owned here rather than by BrowserPanel because the pad grid needs
    // it too, and the browser only exists while its dialog is open. `similarSearch` caches
    // the normalised feature space; it is rebuilt whenever the corpus changes.
    LibraryDb            library;
    SimilarSearch        similarSearch;
    SampleRetirementPool retirementPool;
    Kit                  starterKit;
    Pattern              editPattern;   // the pattern the grid edits (16 lanes -> pads 0..15)
    juce::UndoManager    undoManager;   // undoable step edits (declared after editPattern)

    // The A..H bank. `editPattern` IS the current slot's pattern while you work on it; the
    // bank's own copy is only refreshed when you leave the slot (or save). `pendingSlot` is
    // a switch the engine has accepted but has not reached the bar line for yet.
    PatternBank          bank;
    int                  pendingSlot = -1;
    std::int64_t         switchCountAtQueue = 0;   // see Sequencer::getSwitchCount()

    PadGrid          padGrid;
    TransportBar     transportBar { engine.getSequencer() };
    SequencerGrid    seqGrid { 16, 16 };
    FillBar          fillBar { engine.getSequencer() };
    RollBrushOverlay rollOverlay { 16, 16, SequencerGrid::labelColumnWidth };
    juce::TextButton brushButton { "Roll Brush" };
    juce::TextButton clearRollsButton { "Clear Rolls" };
    juce::ComboBox   rollPresetBox;
    NoteRepeat       noteRepeat;                        // hold a pad to retrigger
    juce::TextButton repeatButton { "Repeat" };        // toggles note-repeat
    juce::ComboBox   repeatRateBox;                     // rate: 1/8 .. 1/32, Build
    MacroKnobs       macroKnobs { engine.getMasterBus() };
    PatternSlots     patternSlots;
    std::array<bool, (size_t) maxLanes> laneLocked {};   // per-lane "keep on reroll" locks
    std::vector<RollBrushOverlay::RollRect> paintedRolls;
    int              autosaveCounter = 0;   // ticks since the last recovery save
    BrandMark        brandMark;
    MasterMeter      masterMeter { engine.getOutputMeter() };
    juce::Label      statusLabel;

    // Painted panel geometry, computed in resized() and drawn in paint(). Keeping the
    // rectangles here is what lets the panels sit BEHIND their child components.
    juce::Rectangle<int> transportPanel, sequencerWell, masterPanel;
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton libraryButton { "Library" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton helpButton { "Help" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton openButton { "Open" };
    juce::TextButton sliceButton { "Slice" };

    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    juce::Component::SafePointer<juce::DialogWindow> libraryWindow;
    juce::Component::SafePointer<juce::DialogWindow> exportWindow;
    juce::Component::SafePointer<juce::DialogWindow> helpWindow;
    std::unique_ptr<juce::FileChooser>               exportChooser;
    std::unique_ptr<juce::FileChooser>               projectChooser;
    std::unique_ptr<juce::FileChooser>               sliceChooser;

    // Full file paths of the sample LAYERS loaded into each pad, in order (empty = the
    // built-in starter synth sound). Tracked so Save/Load can rebuild the kit from disk.
    std::array<juce::StringArray, (size_t) maxLanes> padSourcePaths {};

    // Per-pad state for the inspector's SIMILAR button. `similarAnchor` is the sound the
    // shortlist was computed FROM: pressing Similar repeatedly must walk that one sound's
    // neighbours, not random-walk away from it (and never ping-pong A->B->A). `similarServed`
    // is what the button last installed, so a pad changed by any other route re-anchors.
    std::array<juce::String, (size_t) maxLanes> similarAnchor {};
    std::array<juce::String, (size_t) maxLanes> similarServed {};
    std::array<int,          (size_t) maxLanes> similarCursor {};

    // The sample on the preview pad. Held so the message thread keeps a reference until
    // the next audition retires it (the SampleBuffer.h ownership contract).
    SampleBuffer::Ptr                               previewSample;

    juce::TooltipWindow tooltipWindow { this };   // enables tooltips app-wide (lane locks, sliders)

    std::unique_ptr<FirstRun> firstRun;   // one-time welcome overlay (first launch only)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rollforge
