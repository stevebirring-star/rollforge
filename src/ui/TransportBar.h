#pragma once

// RollForge — TransportBar: play/stop, BPM, tap tempo, swing.
//
// A thin UI over the Sequencer's message-thread control methods. UI only.

#include "engine/Sequencer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class TransportBar final : public juce::Component
{
public:
    explicit TransportBar (Sequencer& sequencer);

    /** Set the tempo control (BPM); drives the sequencer + updates the slider. Used
        by project load. */
    void setTempo (double bpm);

    /** Set the swing control (0..1); drives the sequencer + updates the slider. Used
        by Feel presets and by Make a Beat to apply a genre's swing. */
    void setSwing (float amount);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Fired when the user changes tempo/swing here (slider or tap), so the owner
        can keep the pattern model — the source of truth for export — in sync. */
    std::function<void (double)> onTempoChanged;
    std::function<void (float)>  onSwingChanged;

    /** Drives the displayed tempo/swing from the model (e.g. after loading a
        project): updates the knob AND the live engine, but does NOT fire the
        onTempoChanged/onSwingChanged callbacks (the caller already holds the value). */
    void setDisplayedTempo (double bpm);

    /** Stop from outside — a one-shot song chain reaching its end. The button has its own
        `playing` flag, so setting the sequencer directly would leave it reading "Stop". */
    void stop();

    /** Capture. REC arms it; the box beside it chooses what is being captured. */
    enum class CaptureSource { pads = 0, mic = 1 };

    std::function<void (bool armed, CaptureSource)> onRecordChanged;

    bool isRecording() const noexcept;
    CaptureSource getCaptureSource() const noexcept;

    /** Disarm from outside (the capture finished, or the transport stopped under it). */
    void clearRecord();

    /** Greys out the Mic option when the device gave us no input channel. */
    void setMicAvailable (bool available);

    /** The REC button itself, so the guided tour can cut a hole around it rather than around
        the whole transport. Nothing else should reach in here. */
    juce::Component& getRecordButton() noexcept { return recButton; }
    void setDisplayedSwing (float amount);

private:
    void togglePlay();
    void tapTempo();

    Sequencer& sequencer;

    juce::TextButton playButton { "Play" };
    juce::TextButton tapButton  { "Tap" };
    juce::TextButton recButton  { "REC" };
    juce::ComboBox   sourceBox;
    juce::Slider     bpmSlider;
    juce::Slider     swingSlider;
    juce::Label      bpmCaption;
    juce::Label      swingCaption;

    bool   playing        = false;
    double lastTapMs      = 0.0;
    int    tapCount       = 0;
    double tapIntervalSum = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace rollforge
