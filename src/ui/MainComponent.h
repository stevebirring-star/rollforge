#pragma once

#include "engine/AudioEngine.h"
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
#include "model/RollCompiler.h"
#include "model/RollPresets.h"
#include "model/UndoableActions.h"
#include "ui/PadGrid.h"
#include "ui/NoteRepeat.h"
#include "ui/TransportBar.h"
#include "ui/SequencerGrid.h"
#include "ui/FillBar.h"
#include "ui/RollBrushOverlay.h"
#include "ui/MacroKnobs.h"
#include "ui/BrowserPanel.h"
#include "ui/ExportPanel.h"
#include "ui/AboutView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
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
    void doExportMidi();
    void doExportWav();
    void doExportStems();
    void doSaveProject();
    void doOpenProject();
    void refreshStatus();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;                              // retirement sweep
    void loadFileIntoPad (int padIndex, const juce::File& file);
    void updatePadLabels();
    void updatePadWaveform (int padIndex);   // recompute a pad's waveform thumbnail from its sample
    void updateLaneLabelForPad (int padIndex);   // refresh sequencer lane label(s) targeting this pad
    void refreshPadAudibility();                 // reflect engine mute/solo into pad dimming
    void afterStepEdit (int lane, int step);   // reflect a step change into grid + engine
    void refreshGridFromPattern();             // re-reflect the whole editPattern into the grid
    RollRegion buildBrushRegion (int lane, int startStep, int lengthSteps, float density) const; // roll under the brush (paint + live meter share this)
    Project captureProject();                  // snapshot the session (pattern + FX)
    void    applyProject (const Project&);      // restore a session (pattern + FX)

    // Declared first -> destroyed last: the engine (holding pad sample refs)
    // outlives the Kit/pool/loader that also reference the samples.
    AudioEngine          engine;
    SampleLoader         loader;
    SampleRetirementPool retirementPool;
    Kit                  starterKit;
    Pattern              editPattern;   // the pattern the grid edits (16 lanes -> pads 0..15)
    juce::UndoManager    undoManager;   // undoable step edits (declared after editPattern)

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
    std::array<bool, (size_t) maxLanes> laneLocked {};   // per-lane "keep on reroll" locks
    std::vector<RollBrushOverlay::RollRect> paintedRolls;
    int              autosaveCounter = 0;   // ticks since the last recovery save
    juce::Label      titleLabel;
    juce::Label      statusLabel;
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton libraryButton { "Library" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton helpButton { "Help" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton openButton { "Open" };

    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;
    juce::Component::SafePointer<juce::DialogWindow> libraryWindow;
    juce::Component::SafePointer<juce::DialogWindow> exportWindow;
    juce::Component::SafePointer<juce::DialogWindow> helpWindow;
    std::unique_ptr<juce::FileChooser>               exportChooser;
    std::unique_ptr<juce::FileChooser>               projectChooser;

    // Full file path of the sample loaded into each pad ("" = the built-in starter
    // synth sound). Tracked so Save/Load can rebuild the kit from disk.
    std::array<juce::String, (size_t) maxLanes>      padSourcePath {};

    juce::TooltipWindow tooltipWindow { this };   // enables tooltips app-wide (lane locks, sliders)

    std::unique_ptr<FirstRun> firstRun;   // one-time welcome overlay (first launch only)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rollforge
