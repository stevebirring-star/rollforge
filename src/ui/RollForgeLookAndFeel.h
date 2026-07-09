#pragma once

// RollForge — RollForgeLookAndFeel: the app's hardware.
//
// Every JUCE default widget is replaced here rather than in the components, so the whole
// app changes appearance from one file and nothing has to know what a knob looks like.
//
// The rule the drawing obeys, everywhere, without exception: THE LIGHT COMES FROM ABOVE.
// A raised thing has a pale top edge and a dark bottom edge; a recessed thing has the two
// swapped. One highlight, one direction. Mixed light sources are the thing that makes a
// hand-drawn UI look amateur, and they are the first thing the eye catches.
//
// It is neo-skeuomorphic, not photoreal: a machined skirt, one specular sweep, a crisply
// engraved pointer and a glowing value arc. No chrome gradients, no screws, no bevels on
// things you only read. The sequencer grid and the pads stay flat and legible; only the
// things you TOUCH get metal.
//
// Slider properties this reads (set them on the Slider, not here):
//   "bipolar"   — the value arc grows from 12 o'clock rather than from the minimum.
//   "arcColour" — an explicit uint32 ARGB for the value arc; defaults to Theme::ember.

#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class RollForgeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RollForgeLookAndFeel();

    /** 270 degrees of sweep with the gap at the bottom, the way a real knob is stopped.
        Call this on every rotary slider so the LookAndFeel and the ticks agree. */
    static void styleRotary (juce::Slider& slider, bool bipolar = false);

    /** Paints a raised faceplate: fill, 1px lit top edge, 1px shadowed bottom edge. The
        one place panel geometry is defined, so every panel in the app is the same object. */
    static void drawRaisedPanel (juce::Graphics&, juce::Rectangle<float> bounds, float corner = 6.0f);

    /** Paints a recessed well — the inverse light. Used for the sequencer and meter cavities. */
    static void drawRecessedWell (juce::Graphics&, juce::Rectangle<float> bounds, float corner = 6.0f);

    //==============================================================================
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollForgeLookAndFeel)
};

} // namespace rollforge
