#include "ui/PadInspector.h"

namespace rollforge
{

namespace
{
    const juce::Colour panelText  { 0xffe8e8ec };
    const juce::Colour captionCol { 0xff9a9aa4 };

    void styleKnob (juce::Slider& knob, double lo, double hi, double interval, double initial)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRange (lo, hi, interval);
        knob.setValue (initial, juce::dontSendNotification);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    }

    void styleCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, captionCol);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (11.0f));
    }
}

PadInspector::PadInspector (const juce::String& padName, float tone, float reverbSend)
{
    title.setText (padName, juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, panelText);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (title);

    styleKnob (toneSlider, -1.0, 1.0, 0.01, (double) tone);
    styleKnob (sendSlider,  0.0, 1.0, 0.01, (double) reverbSend);

    toneSlider.setTooltip ("Tilt this pad darker (left) or brighter (right). Centre is flat.");
    sendSlider.setTooltip ("How much of this pad feeds the reverb send. 0 is fully dry.");

    toneSlider.onValueChange = [this] { if (onToneChanged) onToneChanged ((float) toneSlider.getValue()); };
    sendSlider.onValueChange = [this] { if (onSendChanged) onSendChanged ((float) sendSlider.getValue()); };

    addAndMakeVisible (toneSlider);
    addAndMakeVisible (sendSlider);

    styleCaption (toneCaption, "TONE");
    styleCaption (sendCaption, "SEND");
    addAndMakeVisible (toneCaption);
    addAndMakeVisible (sendCaption);

    setSize (196, 132);
}

void PadInspector::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff26262c));
}

void PadInspector::resized()
{
    auto r = getLocalBounds().reduced (8);
    title.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);

    auto captions = r.removeFromTop (14);
    toneCaption.setBounds (captions.removeFromLeft (captions.getWidth() / 2));
    sendCaption.setBounds (captions);

    toneSlider.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    sendSlider.setBounds (r.reduced (4, 0));
}

} // namespace rollforge
