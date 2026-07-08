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
    void setDisplayedSwing (float amount);

private:
    void togglePlay();
    void tapTempo();

    Sequencer& sequencer;

    juce::TextButton playButton { "Play" };
    juce::TextButton tapButton  { "Tap" };
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
