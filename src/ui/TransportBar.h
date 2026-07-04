#pragma once

// RollForge — TransportBar: play/stop, BPM, tap tempo, swing.
//
// A thin UI over the Sequencer's message-thread control methods. UI only.

#include "engine/Sequencer.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class TransportBar final : public juce::Component
{
public:
    explicit TransportBar (Sequencer& sequencer);

    void paint (juce::Graphics&) override;
    void resized() override;

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
