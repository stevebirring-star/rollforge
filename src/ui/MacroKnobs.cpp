#include "ui/MacroKnobs.h"

namespace rollforge
{

MacroKnobs::MacroKnobs (MasterBus& busToUse) : bus (busToUse)
{
    auto setupKnob = [this] (juce::Slider& knob, juce::Label& label, const juce::String& name,
                             double lo, double hi, double interval)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRange (lo, hi, interval);
        knob.setValue (lo < 0.0 ? 0.0 : lo, juce::dontSendNotification);   // bipolar EQ centres at 0
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        addAndMakeVisible (knob);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcc4));
        addAndMakeVisible (label);
    };

    setupKnob (punchKnob, punchLabel, "PUNCH", 0.0, 1.0, 0.01);
    setupKnob (spaceKnob, spaceLabel, "SPACE", 0.0, 1.0, 0.01);
    setupKnob (crushKnob, crushLabel, "CRUSH", 0.0, 1.0, 0.01);
    setupKnob (driveKnob, driveLabel, "DRIVE", 0.0, 1.0, 0.01);
    setupKnob (lowKnob,   lowLabel,   "LOW",  -12.0, 12.0, 0.5);
    setupKnob (midKnob,   midLabel,   "MID",  -12.0, 12.0, 0.5);
    setupKnob (highKnob,  highLabel,  "HIGH", -12.0, 12.0, 0.5);
    setupKnob (compKnob,  compLabel,  "COMP",   0.0,  1.0, 0.01);

    punchKnob.onValueChange = [this] { bus.setPunch ((float) punchKnob.getValue()); };
    spaceKnob.onValueChange = [this] { bus.setSpace ((float) spaceKnob.getValue()); };
    crushKnob.onValueChange = [this] { bus.setCrush ((float) crushKnob.getValue()); };
    driveKnob.onValueChange = [this] { bus.setDrive ((float) driveKnob.getValue()); };
    lowKnob.onValueChange   = [this] { bus.setLowEqDb  ((float) lowKnob.getValue()); };
    midKnob.onValueChange   = [this] { bus.setMidEqDb  ((float) midKnob.getValue()); };
    highKnob.onValueChange  = [this] { bus.setHighEqDb ((float) highKnob.getValue()); };
    compKnob.onValueChange  = [this] { bus.setComp     ((float) compKnob.getValue()); };
}

void MacroKnobs::syncFromBus()
{
    punchKnob.setValue (bus.getPunch(), juce::dontSendNotification);
    spaceKnob.setValue (bus.getSpace(), juce::dontSendNotification);
    crushKnob.setValue (bus.getCrush(), juce::dontSendNotification);
    driveKnob.setValue (bus.getDrive(), juce::dontSendNotification);
    lowKnob.setValue   (bus.getLowEqDb(),  juce::dontSendNotification);
    midKnob.setValue   (bus.getMidEqDb(),  juce::dontSendNotification);
    highKnob.setValue  (bus.getHighEqDb(), juce::dontSendNotification);
    compKnob.setValue  (bus.getComp(),     juce::dontSendNotification);
}

void MacroKnobs::resized()
{
    auto r = getLocalBounds();
    constexpr int cols = 8;
    const int colW = r.getWidth() / cols;

    juce::Slider* knobs[cols]  = { &punchKnob, &spaceKnob, &crushKnob, &driveKnob,
                                   &lowKnob, &midKnob, &highKnob, &compKnob };
    juce::Label*  labels[cols] = { &punchLabel, &spaceLabel, &crushLabel, &driveLabel,
                                   &lowLabel, &midLabel, &highLabel, &compLabel };

    for (int i = 0; i < cols; ++i)
    {
        auto col = (i < cols - 1) ? r.removeFromLeft (colW) : r;
        labels[i]->setBounds (col.removeFromTop (16));
        knobs[i]->setBounds (col.reduced (4, 0));
    }
}

} // namespace rollforge
