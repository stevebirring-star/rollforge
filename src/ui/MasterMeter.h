#pragma once

// RollForge — MasterMeter: two analogue VU needles and a clip lamp, in one milled bezel.
//
// The needle answers "how loud is this?" and the peak bar answers "am I about to clip?".
// They are different instruments and must not be conflated: a VU that jumps on transients
// is a peak meter wearing a VU's face, and it is useless for judging a drum mix.
//
// All the maths lives in engine/OutputMeter — 300 ms symmetric ballistics, and a needle
// that deflects with VOLTAGE rather than with decibels, which is why the printed scale
// crowds -20..0 and opens out over 0..+3. Evenly-spaced dB ticks are the one detail that
// gives a fake meter away, so the ticks are placed with the same function that places the
// needle and the two can never disagree.
//
// UI only: it polls the engine's atomics on a timer and draws.

#include "engine/OutputMeter.h"
#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class MasterMeter final : public juce::Component,
                          public  juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    explicit MasterMeter (const OutputMeter& source);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    /** Draws one needle face (glass, scale, red zone, needle, peak bar) into `face`. */
    void drawFace (juce::Graphics&, juce::Rectangle<float> face, int channel, const juce::String& label);

    static constexpr float sweepDegrees  = 90.0f;   // -45 deg at rest .. +45 deg at +3 VU
    // The brickwall limiter holds the mix at 0.98, which is -0.175 dBFS. A clip lamp that
    // trips at -0.3 would therefore be lit whenever the limiter worked at all — a lamp
    // that is always on tells you nothing. Trip above the ceiling instead.
    static constexpr float clipDbfs      = -0.05f;
    static constexpr int   clipHoldTicks = 30;      // ~1 s at 30 Hz

    const OutputMeter& meter;

    // Smoothed only for DRAWING — the needle's real ballistics are in the engine. This is
    // just the 30 Hz timer catching up with a value that already moves at the right speed.
    float displayed[2] { 0.0f, 0.0f };
    float peakBar[2]   { 0.0f, 0.0f };
    int   clipHold     = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterMeter)
};

} // namespace rollforge
