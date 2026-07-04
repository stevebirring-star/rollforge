#pragma once

#include "engine/AudioEngine.h"
#include "engine/SampleRetirementPool.h"
#include "library/SampleLoader.h"
#include "library/StarterKit.h"
#include "library/KitInstaller.h"
#include "model/RollCompiler.h"
#include "model/RollPresets.h"
#include "model/UndoableActions.h"
#include "ui/PadGrid.h"
#include "ui/TransportBar.h"
#include "ui/SequencerGrid.h"
#include "ui/FillBar.h"
#include "ui/RollBrushOverlay.h"

#include <juce_gui_basics/juce_gui_basics.h>

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
    void openAudioSettings();
    void refreshStatus();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;                              // retirement sweep
    void loadFileIntoPad (int padIndex, const juce::File& file);
    void updatePadLabels();
    void afterStepEdit (int lane, int step);   // reflect a step change into grid + engine
    void refreshGridFromPattern();             // re-reflect the whole editPattern into the grid

    // Declared first -> destroyed last: the engine (holding pad sample refs)
    // outlives the Kit/pool/loader that also reference the samples.
    AudioEngine          engine;
    SampleLoader         loader;
    SampleRetirementPool retirementPool;
    Kit                  starterKit;
    Pattern              editPattern;   // the pattern the grid edits (8 lanes -> pads 0..7)
    juce::UndoManager    undoManager;   // undoable step edits (declared after editPattern)

    PadGrid          padGrid;
    TransportBar     transportBar { engine.getSequencer() };
    SequencerGrid    seqGrid { 8, 16 };
    FillBar          fillBar { engine.getSequencer() };
    RollBrushOverlay rollOverlay { 8, 16, SequencerGrid::labelColumnWidth };
    juce::TextButton brushButton { "Roll Brush" };
    juce::TextButton clearRollsButton { "Clear Rolls" };
    juce::ComboBox   rollPresetBox;
    std::vector<RollBrushOverlay::RollRect> paintedRolls;
    juce::Label      titleLabel;
    juce::Label      statusLabel;
    juce::TextButton settingsButton { "Audio Settings" };

    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rollforge
