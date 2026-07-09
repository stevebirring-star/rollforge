#pragma once

// RollForge — PadInspector: the per-pad TONE + SEND controls, shown in a CallOutBox
// when you right-click a pad.
//
// They live here rather than on the pad face because a pad already carries M, S, R,
// two trim handles, a waveform and a meter; two more inline controls would make an
// 80-pixel tile unusable. A right-click bubble keeps the grid readable and still puts
// the controls on the pad you clicked.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class PadInspector final : public juce::Component
{
public:
    PadInspector (const juce::String& padName, float tone, float reverbSend);

    /** Fired live as the knobs move (the engine applies them without retriggering). */
    std::function<void (float tone)> onToneChanged;
    std::function<void (float send)> onSendChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label  title;
    juce::Slider toneSlider;
    juce::Slider sendSlider;
    juce::Label  toneCaption;
    juce::Label  sendCaption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadInspector)
};

} // namespace rollforge
