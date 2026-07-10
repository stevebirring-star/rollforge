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

    // REC makes something, so it wears the hot accent when armed. It is the only control on
    // this row that does: the transport is the machine running, and it is blue.
    recButton.setClickingTogglesState (true);
    recButton.setColour (juce::TextButton::buttonOnColourId, theme().accentHot);
    recButton.setColour (juce::TextButton::textColourOnId,   theme().background);
    recButton.setTooltip ("Capture what you play into the pattern, quantised to the grid. "
                          "The transport has to be running.");
    recButton.onClick = [this]
    {
        if (onRecordChanged != nullptr)
            onRecordChanged (recButton.getToggleState(), getCaptureSource());
    };
    addAndMakeVisible (recButton);

    sourceBox.addItem ("Pads", 1);
    sourceBox.addItem ("Mic",  2);
    sourceBox.setSelectedId (1, juce::dontSendNotification);
    sourceBox.setTooltip ("Pads: tap the pads or the keys and they land on the grid. "
                          "Mic: beatbox, and the take is turned into kicks, snares and hats.");
    sourceBox.onChange = [this]
    {
        // Changing what you are recording mid-take would leave half a pattern from each.
        if (recButton.getToggleState())
        {
            recButton.setToggleState (false, juce::dontSendNotification);
            if (onRecordChanged != nullptr)
                onRecordChanged (false, getCaptureSource());
        }
    };
    addAndMakeVisible (sourceBox);

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

bool TransportBar::isRecording() const noexcept { return recButton.getToggleState(); }

TransportBar::CaptureSource TransportBar::getCaptureSource() const noexcept
{
    return sourceBox.getSelectedId() == 2 ? CaptureSource::mic : CaptureSource::pads;
}

void TransportBar::clearRecord()
{
    recButton.setToggleState (false, juce::dontSendNotification);
}

void TransportBar::setMicAvailable (bool available)
{
    sourceBox.setItemEnabled (2, available);
    if (! available && sourceBox.getSelectedId() == 2)
        sourceBox.setSelectedId (1, juce::dontSendNotification);
}

void TransportBar::stop()
{
    if (! playing)
        return;

    playing = false;
    sequencer.setPlaying (false);
    playButton.setButtonText ("Play");
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

    // Widths trimmed so REC + its source box fit at the 780 px minimum window width.
    playButton.setBounds (r.removeFromLeft (60));
    r.removeFromLeft (6);
    tapButton.setBounds (r.removeFromLeft (46));
    r.removeFromLeft (10);
    recButton.setBounds (r.removeFromLeft (54));
    r.removeFromLeft (5);
    sourceBox.setBounds (r.removeFromLeft (76));
    r.removeFromLeft (12);

    bpmCaption.setBounds (r.removeFromLeft (34));
    bpmSlider.setBounds (r.removeFromLeft (juce::jmax (80, r.getWidth() - 34 - 120 - 12)));
    r.removeFromLeft (12);

    swingCaption.setBounds (r.removeFromLeft (44));
    swingSlider.setBounds (r);
}

} // namespace rollforge
