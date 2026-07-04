#include "ui/MacroKnobs.h"

namespace rollforge
{

MacroKnobs::MacroKnobs (MasterBus& busToUse) : bus (busToUse)
{
    auto setupKnob = [this] (juce::Slider& knob, juce::Label& label, const juce::String& name)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRange (0.0, 1.0, 0.01);
        knob.setValue (0.0, juce::dontSendNotification);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        addAndMakeVisible (knob);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcc4));
        addAndMakeVisible (label);
    };

    setupKnob (punchKnob, punchLabel, "PUNCH");
    setupKnob (spaceKnob, spaceLabel, "SPACE");
    setupKnob (crushKnob, crushLabel, "CRUSH");
    setupKnob (driveKnob, driveLabel, "DRIVE");

    punchKnob.onValueChange = [this] { bus.setPunch ((float) punchKnob.getValue()); };
    spaceKnob.onValueChange = [this] { bus.setSpace ((float) spaceKnob.getValue()); };
    crushKnob.onValueChange = [this] { bus.setCrush ((float) crushKnob.getValue()); };
    driveKnob.onValueChange = [this] { bus.setDrive ((float) driveKnob.getValue()); };
}

void MacroKnobs::resized()
{
    auto r = getLocalBounds();
    const int colW = r.getWidth() / 4;

    juce::Slider* knobs[4]  = { &punchKnob, &spaceKnob, &crushKnob, &driveKnob };
    juce::Label*  labels[4] = { &punchLabel, &spaceLabel, &crushLabel, &driveLabel };

    for (int i = 0; i < 4; ++i)
    {
        auto col = (i < 3) ? r.removeFromLeft (colW) : r;
        labels[i]->setBounds (col.removeFromTop (16));
        knobs[i]->setBounds (col.reduced (4, 0));
    }
}

} // namespace rollforge
