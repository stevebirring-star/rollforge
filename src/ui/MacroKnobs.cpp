#include "ui/MacroKnobs.h"

#include "ui/GridGeometry.h"
#include "ui/RollForgeLookAndFeel.h"
#include "ui/Theme.h"

namespace rollforge
{

MacroKnobs::MacroKnobs (MasterBus& busToUse) : bus (busToUse)
{
    auto setupKnob = [this] (juce::Slider& knob, juce::Label& label, const juce::String& name,
                             double lo, double hi, double interval)
    {
        // An EQ band is bipolar: its value arc grows out of the centre detent, because
        // flat is the resting state, not -12 dB.
        const bool bipolar = lo < 0.0;
        RollForgeLookAndFeel::styleRotary (knob, bipolar);
        knob.setRange (lo, hi, interval);
        knob.setValue (bipolar ? 0.0 : lo, juce::dontSendNotification);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        addAndMakeVisible (knob);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, theme().textDim);
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

    juce::Slider* knobs[cols]  = { &punchKnob, &spaceKnob, &crushKnob, &driveKnob,
                                   &lowKnob, &midKnob, &highKnob, &compKnob };
    juce::Label*  labels[cols] = { &punchLabel, &spaceLabel, &crushLabel, &driveLabel,
                                   &lowLabel, &midLabel, &highLabel, &compLabel };

    for (int i = 0; i < cols; ++i)
    {
        const auto span = gridSpan (i, cols, r.getWidth(), r.getX());
        auto col = juce::Rectangle<int> (span.getStart(), r.getY(), span.getLength(), r.getHeight());
        labels[i]->setBounds (col.removeFromTop (14));

        // Keep the knob square and centred: a stretched ellipse is the fastest way to
        // make a machined knob look like a sticker.
        auto knobArea = col.removeFromTop (juce::jmax (24, col.getHeight() - 18));
        const int d = juce::jmin (knobArea.getWidth() - 4, knobArea.getHeight());
        knobs[i]->setBounds (juce::Rectangle<int> (d, d + 16).withCentre (
            { knobArea.getCentreX(), knobArea.getCentreY() + 8 }));
    }
}

} // namespace rollforge
