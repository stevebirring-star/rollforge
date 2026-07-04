#pragma once

#include "engine/AudioEngine.h"
#include "engine/SampleRetirementPool.h"
#include "library/SampleLoader.h"
#include "library/StarterKit.h"
#include "library/KitInstaller.h"
#include "ui/PadGrid.h"

#include <juce_gui_basics/juce_gui_basics.h>

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

    // Declared first -> destroyed last: the engine (holding pad sample refs)
    // outlives the Kit/pool/loader that also reference the samples.
    AudioEngine          engine;
    SampleLoader         loader;
    SampleRetirementPool retirementPool;
    Kit                  starterKit;

    PadGrid          padGrid;
    juce::Label      titleLabel;
    juce::Label      statusLabel;
    juce::TextButton settingsButton { "Audio Settings" };

    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rollforge
