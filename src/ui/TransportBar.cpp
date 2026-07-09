#include "ui/TransportBar.h"

#include "ui/Theme.h"

namespace rollforge
{

namespace
{
    inline juce::Colour kPanel()   { return theme().buttonFace; }
    inline juce::Colour kText()     { return theme().text; }
    inline juce::Colour kTextDim()  { return theme().textDim; }
}

TransportBar::TransportBar (Sequencer& seq)
    : sequencer (seq)
{
    playButton.setColour (juce::TextButton::buttonColourId, theme().accentCool);
    playButton.setColour (juce::TextButton::textColourOffId, theme().background);
    playButton.onClick = [this] { togglePlay(); };
    addAndMakeVisible (playButton);

    tapButton.setColour (juce::TextButton::buttonColourId, kPanel());
    tapButton.setColour (juce::TextButton::textColourOffId, kText());
    tapButton.onClick = [this] { tapTempo(); };
    addAndMakeVisible (tapButton);

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setRange (40.0, 300.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
    bpmSlider.onValueChange = [this]
    {
        const double bpm = bpmSlider.getValue();
        sequencer.setTempo (bpm);
        if (onTempoChanged != nullptr)
            onTempoChanged (bpm);
    };
    addAndMakeVisible (bpmSlider);

    swingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    swingSlider.setRange (0.0, 1.0, 0.01);
    swingSlider.setValue (0.0, juce::dontSendNotification);
    swingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    swingSlider.onValueChange = [this]
    {
        const float amount = (float) swingSlider.getValue();
        sequencer.setSwing (amount);
        if (onSwingChanged != nullptr)
            onSwingChanged (amount);
    };
    addAndMakeVisible (swingSlider);

    bpmCaption.setText ("BPM", juce::dontSendNotification);
    bpmCaption.setColour (juce::Label::textColourId, kTextDim());
    bpmCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmCaption);

    swingCaption.setText ("Swing", juce::dontSendNotification);
    swingCaption.setColour (juce::Label::textColourId, kTextDim());
    swingCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (swingCaption);

    sequencer.setTempo (120.0);   // sync the engine to the initial slider value
}

void TransportBar::setTempo (double bpm)
{
    bpmSlider.setValue (juce::jlimit (40.0, 300.0, bpm), juce::sendNotification);
}

void TransportBar::setSwing (float amount)
{
    swingSlider.setValue (juce::jlimit (0.0, 1.0, (double) amount), juce::sendNotification);
}

void TransportBar::togglePlay()
{
    playing = ! playing;
    if (playing)
    {
        sequencer.requestReset();    // play from the top of the loop
        sequencer.setPlaying (true);
    }
    else
    {
        sequencer.setPlaying (false);
    }
    playButton.setButtonText (playing ? "Stop" : "Play");
}

void TransportBar::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double interval = now - lastTapMs;
    lastTapMs = now;

    if (interval > 0.0 && interval < 2000.0)   // within a plausible tap window
    {
        tapIntervalSum += interval;
        ++tapCount;
        const double bpm = 60000.0 / (tapIntervalSum / (double) tapCount);
        bpmSlider.setValue (juce::jlimit (40.0, 300.0, bpm), juce::sendNotification);
    }
    else
    {
        tapCount = 0;             // stale -> restart the averaging
        tapIntervalSum = 0.0;
    }
}

void TransportBar::setDisplayedTempo (double bpm)
{
    // dontSendNotification so we don't re-enter onTempoChanged: the caller already
    // owns the model value. We still push the live engine so playback matches.
    bpmSlider.setValue (juce::jlimit (40.0, 300.0, bpm), juce::dontSendNotification);
    sequencer.setTempo (bpm);
}

void TransportBar::setDisplayedSwing (float amount)
{
    swingSlider.setValue (juce::jlimit (0.0, 1.0, (double) amount), juce::dontSendNotification);
    sequencer.setSwing (amount);
}

void TransportBar::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);   // the faceplate behind the transport is painted by MainComponent
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (6);

    playButton.setBounds (r.removeFromLeft (64));
    r.removeFromLeft (8);
    tapButton.setBounds (r.removeFromLeft (52));
    r.removeFromLeft (14);

    bpmCaption.setBounds (r.removeFromLeft (38));
    bpmSlider.setBounds (r.removeFromLeft (220));
    r.removeFromLeft (14);

    swingCaption.setBounds (r.removeFromLeft (48));
    swingSlider.setBounds (r.removeFromLeft (170));
}

} // namespace rollforge
