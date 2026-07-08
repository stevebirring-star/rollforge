#include "ui/MainComponent.h"

#include "library/KitBuilder.h"
#include "model/ProjectIO.h"
#include "model/Variator.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <utility>
#include <vector>

namespace rollforge
{

namespace colours
{
    static const juce::Colour background { 0xff1a1a1e };
    static const juce::Colour panel      { 0xff26262c };
    static const juce::Colour text       { 0xffe8e8ec };
    static const juce::Colour textDim    { 0xff9a9aa4 };
}

namespace
{
    // Downsample a sample to `bins` |amplitude| peaks (0..1), normalised to the
    // loudest bin, for a pad's waveform thumbnail. Message-thread only (runs at
    // sample-load time, never on the audio thread).
    std::vector<float> computeWaveform (const SampleBuffer& sb, int bins)
    {
        std::vector<float> peaks ((size_t) juce::jmax (1, bins), 0.0f);
        const int n  = sb.getNumSamples();
        const int ch = juce::jmax (1, sb.getNumChannels());
        if (n <= 0)
            return peaks;

        for (int b = 0; b < bins; ++b)
        {
            const int s0 = (int) ((juce::int64) b       * n / bins);
            const int s1 = (int) ((juce::int64) (b + 1) * n / bins);
            float peak = 0.0f;
            for (int s = s0; s < s1; ++s)
            {
                float m = 0.0f;
                for (int c = 0; c < ch; ++c)
                    m += sb.getSample (c, s);
                peak = juce::jmax (peak, std::abs (m / (float) ch));
            }
            peaks[(size_t) b] = peak;
        }

        float mx = 0.0f;
        for (float p : peaks)
            mx = juce::jmax (mx, p);
        if (mx > 1.0e-6f)
            for (float& p : peaks)
                p /= mx;
        return peaks;
    }
}

MainComponent::MainComponent()
{
    setWantsKeyboardFocus (true);

    // Apply the saved UI scale (default 1.0 when there are no settings yet).
    juce::Desktop::getInstance().setGlobalScaleFactor (AppSettings::load().uiScale);

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
    settingsButton.onClick = [this] { openSettings(); };
    addAndMakeVisible (settingsButton);

    libraryButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    libraryButton.setColour (juce::TextButton::textColourOffId, colours::text);
    libraryButton.onClick = [this] { openLibrary(); };
    addAndMakeVisible (libraryButton);

    exportButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    exportButton.setColour (juce::TextButton::textColourOffId, colours::text);
    exportButton.onClick = [this] { openExport(); };
    addAndMakeVisible (exportButton);

    helpButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    helpButton.setColour (juce::TextButton::textColourOffId, colours::text);
    helpButton.onClick = [this] { openHelp(); };
    addAndMakeVisible (helpButton);

    saveButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    saveButton.setColour (juce::TextButton::textColourOffId, colours::text);
    saveButton.onClick = [this] { doSaveProject(); };
    saveButton.setTooltip ("Save the project (kit, pattern, rolls, FX, mute/solo, tempo) to a .rollforge file");
    addAndMakeVisible (saveButton);

    openButton.setColour (juce::TextButton::buttonColourId, colours::panel);
    openButton.setColour (juce::TextButton::textColourOffId, colours::text);
    openButton.onClick = [this] { doOpenProject(); };
    openButton.setTooltip ("Open a .rollforge project");
    addAndMakeVisible (openButton);

    padGrid.onPadTrigger = [this] (int index, float velocity)
    {
        engine.triggerPad (index, velocity);   // the pad flashes itself on click
        // Note-repeat: this mouse-down is hit #1; if Repeat is on, start retriggering.
        noteRepeat.noteOn (index, velocity, engine.getSequencer().getTempo());
    };
    padGrid.onPadRelease = [this] (int index) { noteRepeat.noteOff (index); };
    padGrid.onPadFileDropped = [this] (int index, const juce::File& file)
    {
        loadFileIntoPad (index, file);
    };
    padGrid.onPadMute = [this] (int index, bool muted)
    {
        engine.getDrumEngine().setPadMuted (index, muted);
        refreshPadAudibility();
    };
    padGrid.onPadSolo = [this] (int index, bool soloed)
    {
        engine.getDrumEngine().setPadSoloed (index, soloed);
        refreshPadAudibility();
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
    seqGrid.onLaneLockToggled = [this] (int lane)
    {
        if (lane >= 0 && lane < maxLanes)
        {
            laneLocked[(size_t) lane] = ! laneLocked[(size_t) lane];
            seqGrid.setLaneLocked (lane, laneLocked[(size_t) lane]);
        }
    };
    addAndMakeVisible (seqGrid);

    // FILL / Reroll generate a drum fill into the edit pattern; Humanise is wired
    // straight to the sequencer inside FillBar.
    fillBar.onFill = [this] (FillEngine::Style style, int intensity, std::uint64_t seed)
    {
        // "Make a Beat": generate a full groove into a copy, then swap it in as ONE
        // undoable step so a single Cmd/Ctrl+Z reverts the whole generated beat.
        // Capture the CURRENT audible swing into `before` (swing is a live control,
        // not stored in editPattern) so undo restores the user's swing, not 0.
        Pattern before = editPattern;
        before.swing   = engine.getSequencer().getSwing();
        Pattern after  = before;
        FillEngine::generateFill (after, style, intensity, seed);

        // Lock-and-reroll: any locked lane keeps its current steps instead of the
        // freshly generated ones ("keep the kick, gamble the rest").
        for (int lane = 0; lane < after.numLanes && lane < before.numLanes; ++lane)
            if (laneLocked[(size_t) lane])
                after.lane (lane) = before.lane (lane);

        // refresh runs on BOTH perform and undo, so applying the (restored) pattern's
        // swing here keeps the transport + engine swing in sync through undo/redo:
        // the genre's curated swing is audible on generate; undo puts the user's back.
        auto refresh = [this]
        {
            refreshGridFromPattern();
            paintedRolls.clear();             // generated rolls play but aren't drawn on the grid
            rollOverlay.setRolls (paintedRolls);
            transportBar.setSwing (editPattern.swing);   // drives sequencer.setSwing via the slider
            engine.getSequencer().setPattern (editPattern);
        };

        undoManager.beginNewTransaction();
        undoManager.perform (new SetPatternAction (editPattern, before, after, refresh));
    };
    // "Vary": mutate the CURRENT groove instead of regenerating it — a few hits on/
    // off, ghost notes, accents — as one undoable step, then flash what changed.
    // Locked lanes are honoured (the Variator skips them).
    fillBar.onVary = [this] (int intensity, std::uint64_t seed)
    {
        Pattern before = editPattern;
        Pattern after  = editPattern;
        const float amount   = juce::jlimit (0.0f, 1.0f, (float) intensity / 5.0f);
        const auto  changes  = Variator::vary (after, amount, seed, laneLocked);

        auto refresh = [this]
        {
            refreshGridFromPattern();
            engine.getSequencer().setPattern (editPattern);
        };

        undoManager.beginNewTransaction();
        undoManager.perform (new SetPatternAction (editPattern, before, after, refresh));

        std::vector<std::pair<int, int>> cells;
        cells.reserve (changes.size());
        for (const auto& c : changes)
            cells.push_back ({ c.lane, c.step });
        seqGrid.flashChanged (cells);
    };
    // Feel presets apply the Humaniser inside FillBar and hand back the swing so the
    // transport's swing control stays in sync.
    fillBar.onFeelSwing = [this] (float swing) { transportBar.setSwing (swing); };
    addAndMakeVisible (fillBar);

    // Roll brush: toggle it on, then drag across a lane to paint an accelerating
    // roll (drag up = denser). Clear Rolls removes them.
    brushButton.setClickingTogglesState (true);
    brushButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a7a74));
    brushButton.onClick = [this] { rollOverlay.setBrushEnabled (brushButton.getToggleState()); };
    addAndMakeVisible (brushButton);

    clearRollsButton.onClick = [this]
    {
        editPattern.numRolls = 0;
        paintedRolls.clear();
        rollOverlay.setRolls (paintedRolls);
        engine.getSequencer().setPattern (editPattern);
    };
    addAndMakeVisible (clearRollsButton);

    rollPresetBox.addItem ("Auto (density)", 1);
    for (int i = 0; i < RollPresets::NumPresets; ++i)
        rollPresetBox.addItem (RollPresets::name ((RollPresets::Preset) i), i + 2);
    rollPresetBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (rollPresetBox);

    // Live note-repeat: hold a pad to retrigger at the chosen rate (Build = an
    // accelerating, crescendoing roll). Repeat hits reuse the pad-trigger path.
    noteRepeat.onHit = [this] (int pad, float velocity)
    {
        engine.triggerPad (pad, velocity);
        padGrid.flashPad (pad);
    };
    repeatButton.setClickingTogglesState (true);
    repeatButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4cc2ff));
    repeatButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    repeatButton.setTooltip ("Hold a pad to retrigger it at the chosen rate");
    repeatButton.onClick = [this] { noteRepeat.setEnabled (repeatButton.getToggleState()); };
    addAndMakeVisible (repeatButton);

    for (int i = 0; i < NoteRepeat::NumRates; ++i)
        repeatRateBox.addItem (NoteRepeat::rateName ((NoteRepeat::Rate) i), i + 1);
    repeatRateBox.setSelectedId ((int) NoteRepeat::Sixteenth + 1, juce::dontSendNotification);
    repeatRateBox.setTooltip ("Note-repeat rate");
    repeatRateBox.onChange = [this]
    {
        noteRepeat.setRate ((NoteRepeat::Rate) juce::jlimit (0, NoteRepeat::NumRates - 1,
                                                             repeatRateBox.getSelectedId() - 1));
    };
    addAndMakeVisible (repeatRateBox);

    rollOverlay.onRollPainted = [this] (int lane, int startStep, int length, float density)
    {
        if (lane < 0 || lane >= editPattern.numLanes || editPattern.numRolls >= maxRolls)
            return;

        editPattern.rolls[(std::size_t) editPattern.numRolls]
            = RollCompiler::compile (buildBrushRegion (lane, startStep, length, density));
        ++editPattern.numRolls;

        paintedRolls.push_back ({ lane, startStep, juce::jmax (1, length) });
        rollOverlay.setRolls (paintedRolls);
        engine.getSequencer().setPattern (editPattern);
    };

    // Live meter: how many hits the roll under the brush would produce right now
    // (same region-builder as the paint above, so the preview matches the result).
    rollOverlay.getHitCount = [this] (int lane, int startStep, int length, float density)
    {
        return RollCompiler::compile (buildBrushRegion (lane, startStep, length, density)).count;
    };
    addAndMakeVisible (rollOverlay);   // added after seqGrid -> drawn on top

    addAndMakeVisible (macroKnobs);

    engine.initialise();

    // Install the synthesised starter kit so pads play real drum sounds. Built
    // once at a fixed reference rate; the Voice resamples per-voice to the device
    // rate, so it never needs rebuilding on a device change.
    starterKit = StarterKit::build (44100.0);
    installKitIntoEngine (starterKit, engine.getDrumEngine());
    updatePadLabels();
    refreshPadAudibility();

    // Editable sequencer pattern: 16 lanes, each targeting pads 0..15, all off.
    editPattern.numLanes = 16;
    for (int lane = 0; lane < 16; ++lane)
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

    // Crash recovery: a recovery file present at launch means the previous session
    // did not exit cleanly -- restore its pattern + FX.
    if (Autosave::hasRecovery())
    {
        Project recovered;
        if (Autosave::load (recovered))
            applyProject (recovered);
    }

    startTimer (33);   // ~30 Hz: reclaim retired buffers + drive the playhead
    setSize (780, 880);

    // One-time welcome overlay: shown only on the very first launch (gated by a
    // marker file in app-data). It dims the app and lists a few tips; dismissing it
    // writes the marker so it never returns.
    if (FirstRunState::shouldShow())
    {
        firstRun = std::make_unique<FirstRun>();
        firstRun->onDismissed = [this]
        {
            FirstRunState::markShown();
            // Delete the overlay *after* this button-click callback unwinds (deleting
            // it synchronously would destroy the button mid-click). SafePointer guards
            // against the window closing before the async fires.
            juce::Component::SafePointer<MainComponent> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->firstRun.reset(); });
        };
        addAndMakeVisible (*firstRun);         // added last -> drawn on top of everything
        firstRun->setBounds (getLocalBounds());
    }
}

MainComponent::~MainComponent()
{
    stopTimer();

    // Clean exit -> drop the recovery file so the next launch starts fresh.
    Autosave::clear();

    // Stop listening before any teardown so no callback lands on a half-dead component.
    engine.getDeviceManager().removeChangeListener (this);

    if (settingsWindow != nullptr)
        settingsWindow.deleteAndZero();

    if (libraryWindow != nullptr)
        libraryWindow.deleteAndZero();

    if (exportWindow != nullptr)
        exportWindow.deleteAndZero();

    if (helpWindow != nullptr)
        helpWindow.deleteAndZero();

    engine.shutdown();   // stops the audio thread before members (and samples) are destroyed
}

void MainComponent::updatePadLabels()
{
    for (int i = 0; i < kitNumPads; ++i)
        if (auto sample = starterKit.pad (i).primarySample())
        {
            padGrid.setPadLabel (i, sample->getName());
            padGrid.setPadWaveform (i, computeWaveform (*sample, 48));
        }
}

void MainComponent::updatePadWaveform (int padIndex)
{
    if (padIndex < 0 || padIndex >= kitNumPads)
        return;
    if (auto sample = starterKit.pad (padIndex).primarySample())
        padGrid.setPadWaveform (padIndex, computeWaveform (*sample, 48));
    else
        padGrid.setPadWaveform (padIndex, {});
}

void MainComponent::updateLaneLabelForPad (int padIndex)
{
    // Keep the sequencer row label in step with the pad's sound after a load / NEW
    // KIT (otherwise the rows keep their starter-kit names until the next Make a
    // Beat). Uses the sample name, matching refreshGridFromPattern().
    if (padIndex < 0 || padIndex >= kitNumPads)
        return;
    juce::String label;
    if (auto sample = starterKit.pad (padIndex).primarySample())
        label = sample->getName();
    for (int lane = 0; lane < editPattern.numLanes; ++lane)
        if (editPattern.lane (lane).targetPad == padIndex)
            seqGrid.setLaneLabel (lane, label);
}

void MainComponent::refreshPadAudibility()
{
    auto& drum = engine.getDrumEngine();
    for (int p = 0; p < kitNumPads; ++p)
        padGrid.setPadAudible (p, drum.isPadAudible (p));
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

RollRegion MainComponent::buildBrushRegion (int lane, int startStep, int lengthSteps, float density) const
{
    const int pad = (lane >= 0 && lane < editPattern.numLanes) ? editPattern.lane (lane).targetPad : 0;
    const int len = juce::jmax (1, lengthSteps);
    const int sel = rollPresetBox.getSelectedId();

    if (sel <= 1)   // Auto: an accelerating roll whose end density follows the vertical drag
    {
        RollRegion r;
        r.startStep   = startStep;
        r.lengthSteps = (double) len;
        r.targetPad   = pad;
        r.speed       = { 2.0f, 2.0f + density * 14.0f, 0.3f };
        r.volume      = { 1.0f, 0.7f, 0.0f };
        r.pitch       = { 0.0f, 0.0f, 0.0f };
        return r;
    }
    return RollPresets::make ((RollPresets::Preset) (sel - 2), startStep, (double) len, pad);
}

Project MainComponent::captureProject()
{
    Project p;
    p.pattern = editPattern;

    // Tempo + swing are live sequencer controls; capture them as the authoritative
    // transport, mirrored into the pattern so the saved file is self-consistent.
    auto& seq = engine.getSequencer();
    p.bpm   = p.pattern.bpm   = seq.getTempo();
    p.swing = p.pattern.swing = seq.getSwing();

    auto& bus = engine.getMasterBus();
    p.punch = bus.getPunch(); p.drive = bus.getDrive();
    p.crush = bus.getCrush(); p.space = bus.getSpace();

    auto& drum = engine.getDrumEngine();
    for (int i = 0; i < kitNumPads && i < projectNumPads; ++i)
    {
        ProjectPad& pad = p.pads[(size_t) i];
        pad.samplePath = padSourcePath[(size_t) i];   // full path; "" = starter synth sound
        pad.chokeGroup = starterKit.pad (i).chokeGroup;
        pad.muted      = drum.isPadMuted (i);
        pad.soloed     = drum.isPadSoloed (i);
    }
    return p;
}

void MainComponent::applyProject (const Project& p)
{
    editPattern = p.pattern;

    // Rebuild the kit from the saved per-pad paths (RT-safe per-pad swap). An empty
    // path — or a file that no longer exists — restores the built-in starter sound.
    Kit fresh = StarterKit::build (44100.0);
    auto& drum = engine.getDrumEngine();
    for (int i = 0; i < kitNumPads && i < projectNumPads; ++i)
    {
        const juce::String path = p.pads[(size_t) i].samplePath;
        SampleBuffer::Ptr  sample;
        juce::String       label;

        if (path.isNotEmpty())
            if (auto loaded = loader.loadFile (juce::File (path)))
            {
                sample = loaded;
                label  = juce::File (path).getFileNameWithoutExtension();
                padSourcePath[(size_t) i] = path;
            }

        if (sample == nullptr)   // empty path, or the file is gone -> starter sound
        {
            sample = fresh.pad (i).primarySample();
            label  = (sample != nullptr) ? sample->getName() : juce::String();
            padSourcePath[(size_t) i] = juce::String();
        }

        if (sample != nullptr)
        {
            starterKit.pad (i).chokeGroup = p.pads[(size_t) i].chokeGroup;
            installSampleIntoPad (retirementPool, starterKit, engine.getDrumEngine(), i, sample);
            padGrid.setPadLabel (i, label);
            updatePadWaveform (i);
        }

        drum.setPadMuted  (i, p.pads[(size_t) i].muted);
        drum.setPadSoloed (i, p.pads[(size_t) i].soloed);
        padGrid.setPadMuted  (i, p.pads[(size_t) i].muted);
        padGrid.setPadSoloed (i, p.pads[(size_t) i].soloed);
    }

    refreshGridFromPattern();
    refreshPadAudibility();
    engine.getSequencer().setPattern (editPattern);

    transportBar.setTempo (p.bpm);
    transportBar.setSwing (p.swing);

    auto& bus = engine.getMasterBus();
    bus.setPunch (p.punch); bus.setDrive (p.drive);
    bus.setCrush (p.crush); bus.setSpace (p.space);
    macroKnobs.syncFromBus();   // reflect the restored macro values into the knobs
}

void MainComponent::loadFileIntoPad (int padIndex, const juce::File& file)
{
    // Decode on the message thread (fast for typical drum one-shots), then swap
    // the pad's sample via the RT-safe retire-old / install-new path.
    if (auto sample = loader.loadFile (file))
    {
        installSampleIntoPad (retirementPool, starterKit, engine.getDrumEngine(), padIndex, sample);
        if (padIndex >= 0 && padIndex < (int) padSourcePath.size())
            padSourcePath[(size_t) padIndex] = file.getFullPathName();
        padGrid.setPadLabel (padIndex, file.getFileNameWithoutExtension());
        updatePadWaveform (padIndex);
        updateLaneLabelForPad (padIndex);
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

    // Drive the per-pad level meters (called every tick so silent pads decay too).
    auto& drum = engine.getDrumEngine();
    for (int p = 0; p < kitNumPads; ++p)
        padGrid.setPadLevel (p, drum.getPadLevel (p));

    // Autosave a recovery file every ~60 s (30 Hz timer -> 1800 ticks).
    if (++autosaveCounter >= 1800)
    {
        autosaveCounter = 0;
        Autosave::save (captureProject());
    }
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

void MainComponent::openSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    auto view = std::make_unique<SettingsView> (engine.getDeviceManager());
    view->onScaleChanged = [] (float scale)
    {
        juce::Desktop::getInstance().setGlobalScaleFactor (scale);
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (view.release());
    options.dialogTitle                  = "Settings";
    options.dialogBackgroundColour       = colours::background;
    options.componentToCentreAround      = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = true;

    settingsWindow = options.launchAsync();
}

void MainComponent::openLibrary()
{
    if (libraryWindow != nullptr)
    {
        libraryWindow->toFront (true);
        return;
    }

    auto browser = std::make_unique<BrowserPanel>();
    browser->onNewKit = [this] (const std::array<juce::String, kitNumPads>& paths)
    {
        for (int p = 0; p < kitNumPads; ++p)
        {
            if (paths[(size_t) p].isEmpty())
                continue;
            const juce::File file (paths[(size_t) p]);
            if (auto sample = loader.loadFile (file))
            {
                // Auto-choke hats (closed cuts open) before installing, so the pad's
                // choke group is carried into the engine with the new sample.
                starterKit.pad (p).chokeGroup = KitBuilder::chokeGroupForPad (p);
                installSampleIntoPad (retirementPool, starterKit, engine.getDrumEngine(), p, sample);
                padSourcePath[(size_t) p] = file.getFullPathName();
                padGrid.setPadLabel (p, file.getFileNameWithoutExtension());
                updatePadWaveform (p);
                updateLaneLabelForPad (p);
            }
        }
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (browser.release());
    options.dialogTitle                  = "Sample Library";
    options.dialogBackgroundColour       = colours::background;
    options.componentToCentreAround      = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = true;

    libraryWindow = options.launchAsync();
}

void MainComponent::openExport()
{
    if (exportWindow != nullptr)
    {
        exportWindow->toFront (true);
        return;
    }

    auto panel = std::make_unique<ExportPanel>();
    panel->onExportMidi  = [this] { doExportMidi(); };
    panel->onExportWav   = [this] { doExportWav(); };
    panel->onExportStems = [this] { doExportStems(); };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (panel.release());
    options.dialogTitle                  = "Export";
    options.dialogBackgroundColour       = colours::background;
    options.componentToCentreAround      = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    exportWindow = options.launchAsync();
}

void MainComponent::openHelp()
{
    if (helpWindow != nullptr)
    {
        helpWindow->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new AboutView());
    options.dialogTitle                  = "Help & About";
    options.dialogBackgroundColour       = colours::background;
    options.componentToCentreAround      = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = true;
    helpWindow = options.launchAsync();
}

void MainComponent::doExportMidi()
{
    exportChooser = std::make_unique<juce::FileChooser> ("Export MIDI", juce::File(), "*.mid");
    exportChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File())
                return;
            if (! f.hasFileExtension ("mid"))
                f = f.withFileExtension ("mid");
            MidiExporter::save (editPattern, f, 1);
        });
}

void MainComponent::doExportWav()
{
    exportChooser = std::make_unique<juce::FileChooser> ("Export WAV (mix)", juce::File(), "*.wav");
    exportChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File())
                return;
            if (! f.hasFileExtension ("wav"))
                f = f.withFileExtension ("wav");

            // Render on a fresh engine (with the current kit) so the live audio
            // thread is never touched.
            DrumEngine exportEngine;
            installKitIntoEngine (starterKit, exportEngine);

            OfflineRenderer::Options opts;
            opts.sampleRate = 44100.0;
            opts.bars = 1;
            auto& bus = engine.getMasterBus();
            opts.punch = bus.getPunch(); opts.drive = bus.getDrive();
            opts.crush = bus.getCrush(); opts.space = bus.getSpace();
            WavExporter::exportMix (exportEngine, editPattern, f, opts);
        });
}

void MainComponent::doExportStems()
{
    exportChooser = std::make_unique<juce::FileChooser> ("Choose a folder for the stems");
    exportChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (! dir.isDirectory())
                return;

            DrumEngine exportEngine;
            installKitIntoEngine (starterKit, exportEngine);

            OfflineRenderer::Options opts;
            opts.bars = 1;
            WavExporter::exportStems (exportEngine, editPattern, dir, opts);
        });
}

void MainComponent::doSaveProject()
{
    projectChooser = std::make_unique<juce::FileChooser> ("Save Project", juce::File(), "*.rollforge");
    projectChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                     | juce::FileBrowserComponent::canSelectFiles
                                     | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File())
                return;
            if (! f.hasFileExtension ("rollforge"))
                f = f.withFileExtension ("rollforge");
            ProjectIO::save (captureProject(), f);
        });
}

void MainComponent::doOpenProject()
{
    projectChooser = std::make_unique<juce::FileChooser> ("Open Project", juce::File(), "*.rollforge");
    projectChooser->launchAsync (juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile())
                return;
            Project loaded;
            if (ProjectIO::load (f, loaded))
                applyProject (loaded);
        });
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
    statusRow.removeFromRight (8);
    libraryButton.setBounds (statusRow.removeFromRight (80));
    statusRow.removeFromRight (8);
    exportButton.setBounds (statusRow.removeFromRight (72));
    statusRow.removeFromRight (8);
    saveButton.setBounds (statusRow.removeFromRight (58));
    statusRow.removeFromRight (6);
    openButton.setBounds (statusRow.removeFromRight (58));
    statusRow.removeFromRight (8);
    helpButton.setBounds (statusRow.removeFromRight (64));
    statusRow.removeFromRight (12);
    statusLabel.setBounds (statusRow);

    area.removeFromTop (12);
    transportBar.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);
    fillBar.setBounds (area.removeFromTop (32));
    area.removeFromTop (8);

    auto rollRow = area.removeFromTop (28);
    brushButton.setBounds (rollRow.removeFromLeft (110));
    rollRow.removeFromLeft (6);
    clearRollsButton.setBounds (rollRow.removeFromLeft (110));
    rollRow.removeFromLeft (10);
    rollPresetBox.setBounds (rollRow.removeFromLeft (160));
    // Note-repeat controls on the right of the roll row.
    repeatRateBox.setBounds (rollRow.removeFromRight (96));
    rollRow.removeFromRight (6);
    repeatButton.setBounds (rollRow.removeFromRight (84));

    area.removeFromTop (8);
    area.removeFromBottom (26);   // leave room for the hint text

    const auto gridBounds = area.removeFromTop ((int) (area.getHeight() * 0.50f));
    seqGrid.setBounds (gridBounds);
    rollOverlay.setBounds (gridBounds);   // exactly overlaps the grid
    area.removeFromTop (8);
    macroKnobs.setBounds (area.removeFromTop (74));
    area.removeFromTop (8);
    padGrid.setBounds (area);

    if (firstRun != nullptr)
        firstRun->setBounds (getLocalBounds());   // overlay always covers the whole window
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

    // Space auditions the kick (pad 0) — matches the on-screen hint + Help text.
    if (key == juce::KeyPress::spaceKey)
    {
        engine.triggerPad (0, 1.0f);
        padGrid.flashPad (0);
        return true;
    }

    // 1234 / qwer / asdf / zxcv mirror the 4x4 grid.
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
