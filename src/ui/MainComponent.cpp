#include "ui/MainComponent.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace rollforge
{

namespace colours
{
    static const juce::Colour background { 0xff1a1a1e };
    static const juce::Colour panel      { 0xff26262c };
    static const juce::Colour text       { 0xffe8e8ec };
    static const juce::Colour textDim    { 0xff9a9aa4 };
}

MainComponent::MainComponent()
{
    setWantsKeyboardFocus (true);

    titleLabel.setText ("RollForge", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, colours::textDim);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    settingsButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    settingsButton.setColour (juce::TextButton::textColourOffId, colours::text);
    settingsButton.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible (settingsButton);

    padGrid.onPadTrigger = [this] (int index, float velocity)
    {
        engine.triggerPad (index, velocity);   // the pad flashes itself on click
    };
    padGrid.onPadFileDropped = [this] (int index, const juce::File& file)
    {
        loadFileIntoPad (index, file);
    };
    addAndMakeVisible (padGrid);
    addAndMakeVisible (transportBar);

    seqGrid.onGestureStart = [this] { undoManager.beginNewTransaction(); };
    seqGrid.onStepEdit = [this] (int lane, int step, bool on, float velocity)
    {
        if (lane >= 0 && lane < editPattern.numLanes && step >= 0 && step < maxStepsPerLane)
        {
            const Step before = editPattern.lane (lane).step (step);
            Step after = before;
            after.on = on;
            after.velocity = velocity;
            undoManager.perform (new SetStepAction (editPattern, lane, step, before, after,
                                                    [this] (int l, int s) { afterStepEdit (l, s); }));
        }
    };
    addAndMakeVisible (seqGrid);

    // FILL / Reroll generate a drum fill into the edit pattern; Humanise is wired
    // straight to the sequencer inside FillBar.
    fillBar.onFill = [this] (FillEngine::Style style, int intensity, std::uint64_t seed)
    {
        FillEngine::generateFill (editPattern, style, intensity, seed);
        refreshGridFromPattern();
        engine.getSequencer().setPattern (editPattern);
    };
    addAndMakeVisible (fillBar);

    engine.initialise();

    // Install the synthesised starter kit so pads play real drum sounds. Built
    // once at a fixed reference rate; the Voice resamples per-voice to the device
    // rate, so it never needs rebuilding on a device change.
    starterKit = StarterKit::build (44100.0);
    installKitIntoEngine (starterKit, engine.getDrumEngine());
    updatePadLabels();

    // Editable sequencer pattern: 8 lanes, each targeting pads 0..7, all off.
    editPattern.numLanes = 8;
    for (int lane = 0; lane < 8; ++lane)
    {
        editPattern.lane (lane).targetPad = lane;
        editPattern.lane (lane).length = 16;
        if (auto sample = starterKit.pad (lane).primarySample())
            seqGrid.setLaneLabel (lane, sample->getName());
        for (int step = 0; step < 16; ++step)
            seqGrid.setStep (lane, step, false, 0.8f);
    }
    engine.getSequencer().setPattern (editPattern);

    engine.getDeviceManager().addChangeListener (this);
    refreshStatus();

    startTimer (33);   // ~30 Hz: reclaim retired buffers + drive the playhead
    setSize (780, 770);
}

MainComponent::~MainComponent()
{
    stopTimer();

    // Stop listening before any teardown so no callback lands on a half-dead component.
    engine.getDeviceManager().removeChangeListener (this);

    if (settingsWindow != nullptr)
        settingsWindow.deleteAndZero();

    engine.shutdown();   // stops the audio thread before members (and samples) are destroyed
}

void MainComponent::updatePadLabels()
{
    for (int i = 0; i < kitNumPads; ++i)
        if (auto sample = starterKit.pad (i).primarySample())
            padGrid.setPadLabel (i, sample->getName());
}

void MainComponent::afterStepEdit (int lane, int step)
{
    const Step& s = editPattern.lane (lane).step (step);
    seqGrid.setStep (lane, step, s.on, s.velocity);
    engine.getSequencer().setPattern (editPattern);
}

void MainComponent::refreshGridFromPattern()
{
    const int rows = seqGrid.getNumLanes();
    for (int lane = 0; lane < rows; ++lane)
    {
        if (lane < editPattern.numLanes)
        {
            const int pad = editPattern.lane (lane).targetPad;
            juce::String label;
            if (pad >= 0 && pad < kitNumPads)
                if (auto sample = starterKit.pad (pad).primarySample())
                    label = sample->getName();
            seqGrid.setLaneLabel (lane, label);

            for (int step = 0; step < 16; ++step)
            {
                const Step& s = editPattern.lane (lane).step (step);
                seqGrid.setStep (lane, step, s.on, s.velocity);
            }
        }
        else
        {
            seqGrid.setLaneLabel (lane, {});
            for (int step = 0; step < 16; ++step)
                seqGrid.setStep (lane, step, false, 0.8f);
        }
    }
}

void MainComponent::loadFileIntoPad (int padIndex, const juce::File& file)
{
    // Decode on the message thread (fast for typical drum one-shots), then swap
    // the pad's sample via the RT-safe retire-old / install-new path.
    if (auto sample = loader.loadFile (file))
    {
        installSampleIntoPad (retirementPool, starterKit, engine.getDrumEngine(), padIndex, sample);
        padGrid.setPadLabel (padIndex, file.getFileNameWithoutExtension());
        padGrid.flashPad (padIndex);
    }
}

void MainComponent::timerCallback()
{
    // Reclaim retired sample buffers that no voice references any more.
    retirementPool.sweep();

    // Drive the sequencer playhead highlight.
    auto& seq = engine.getSequencer();
    const int nSteps = seqGrid.getNumSteps();
    const int step = (seq.isPlaying() && seq.getCurrentStep() >= 0)
                       ? (int) (seq.getCurrentStep() % nSteps)
                       : -1;
    seqGrid.setPlayheadStep (step);
}

void MainComponent::refreshStatus()
{
    if (auto* device = engine.getDeviceManager().getCurrentAudioDevice())
    {
        statusLabel.setText ("Audio: " + device->getName()
                                 + "  •  " + juce::String (device->getCurrentSampleRate(), 0) + " Hz"
                                 + "  •  " + juce::String (device->getCurrentBufferSizeSamples()) + " samples",
                             juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("No audio device open — open Audio Settings to choose one.",
                             juce::dontSendNotification);
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &engine.getDeviceManager())
        refreshStatus();
}

void MainComponent::openAudioSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        engine.getDeviceManager(),
        /*minInputChannels*/  0, /*maxInputChannels*/  0,
        /*minOutputChannels*/ 1, /*maxOutputChannels*/ 2,
        /*showMidiInput*/     false,
        /*showMidiOutput*/    false,
        /*showChannelsAsStereoPairs*/ true,
        /*hideAdvancedOptionsWithButton*/ false);
    selector->setSize (500, 420);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector.release());
    options.dialogTitle              = "Audio Settings";
    options.dialogBackgroundColour   = colours::background;
    options.componentToCentreAround  = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar        = true;
    options.resizable                = true;

    settingsWindow = options.launchAsync();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Click a pad to play. Drag an audio file onto a pad to load it. Space plays the kick.",
                getLocalBounds().reduced (20).removeFromBottom (22),
                juce::Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    titleLabel.setBounds (area.removeFromTop (40));
    area.removeFromTop (6);

    auto statusRow = area.removeFromTop (26);
    settingsButton.setBounds (statusRow.removeFromRight (150));
    statusRow.removeFromRight (12);
    statusLabel.setBounds (statusRow);

    area.removeFromTop (12);
    transportBar.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);
    fillBar.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);
    area.removeFromBottom (26);   // leave room for the hint text

    seqGrid.setBounds (area.removeFromTop ((int) (area.getHeight() * 0.58f)));
    area.removeFromTop (10);
    padGrid.setBounds (area);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Cmd/Ctrl+Z undo, Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y redo.
    const auto mods = key.getModifiers();
    if (mods.isCommandDown())
    {
        const int code = key.getKeyCode();
        if (code == 'Z') { mods.isShiftDown() ? undoManager.redo() : undoManager.undo(); return true; }
        if (code == 'Y') { undoManager.redo(); return true; }
        return false;
    }

    // 1234 / qwer / asdf / zxcv mirror the 4x4 grid. Space is left unhandled
    // (reserved for play/stop later).
    const int pad = keyCharToPad (juce::CharacterFunctions::toLowerCase (key.getTextCharacter()));
    if (pad >= 0)
    {
        engine.triggerPad (pad, 1.0f);
        padGrid.flashPad (pad);
        return true;
    }

    return false;
}

} // namespace rollforge
