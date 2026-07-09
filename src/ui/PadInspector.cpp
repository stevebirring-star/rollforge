#include "ui/PadInspector.h"
#include "ui/RollForgeLookAndFeel.h"
#include "ui/Theme.h"

namespace rollforge
{

namespace
{
    const juce::Colour panelText  { 0xffe8e8ec };
    const juce::Colour captionCol { 0xff9a9aa4 };

    void styleKnob (juce::Slider& knob, double lo, double hi, double interval, double initial,
                    bool bipolar)
    {
        // TONE is a tilt about a centre detent, so its arc has to grow out of 12 o'clock;
        // SEND only ever goes up from nothing. Drawn any other way, a flat tone reads as a
        // knob that is already a third of the way open.
        RollForgeLookAndFeel::styleRotary (knob, bipolar);
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

PadInspector::PadInspector (const juce::String& padName, float tone, float reverbSend,
                            int numLayers, LayerMode layerMode, bool canFindSimilar)
{
    title.setText (padName, juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, panelText);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (title);

    styleKnob (toneSlider, -1.0, 1.0, 0.01, (double) tone,       true);
    styleKnob (sendSlider,  0.0, 1.0, 0.01, (double) reverbSend, false);

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

    // A one-sample pad has nothing to choose between, so it doesn't ask.
    const bool layered = numLayers > 1;
    if (layered)
    {
        styleCaption (layersCaption, juce::String (numLayers) + " layers");
        addAndMakeVisible (layersCaption);

        layerModeBox.addItem ("Round-robin", (int) LayerMode::roundRobin + 1);
        layerModeBox.addItem ("By velocity", (int) LayerMode::velocity   + 1);
        layerModeBox.setSelectedId ((int) layerMode + 1, juce::dontSendNotification);
        layerModeBox.setTooltip ("Round-robin cycles the layers so repeats don't machine-gun; "
                                 "By velocity picks a layer from how hard the hit is.");
        layerModeBox.onChange = [this]
        {
            if (onLayerModeChanged)
                onLayerModeChanged ((LayerMode) (layerModeBox.getSelectedId() - 1));
        };
        addAndMakeVisible (layerModeBox);
    }

    // Only offered when the pad holds a library sample and the library holds others. The
    // button is always present so the bubble doesn't change size between pads; it is just
    // dead when there is nothing to search.
    similarButton.setEnabled (canFindSimilar);
    similarButton.setTooltip (canFindSimilar
        ? "Swap this pad for the closest sound of the same kind in your library. "
          "Press again to step to the next one."
        : "Scan a samples folder and load this pad from the library to find similar sounds");
    similarButton.onClick = [this] { findSimilar(); };
    addAndMakeVisible (similarButton);

    setSize (196, layered ? 212 : 166);
}

void PadInspector::findSimilar()
{
    if (onSimilar == nullptr)
        return;

    const auto newName = onSimilar();
    if (newName.isEmpty())
    {
        // The library has no other sound of this kind. Saying so once beats a button that
        // silently does nothing however often it is pressed.
        similarButton.setEnabled (false);
        similarButton.setButtonText ("No similar sounds");
        return;
    }

    title.setText (newName, juce::dontSendNotification);
}

void PadInspector::paint (juce::Graphics& g)
{
    g.fillAll (theme().panelRaised);
}

void PadInspector::resized()
{
    auto r = getLocalBounds().reduced (8);
    title.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);

    similarButton.setBounds (r.removeFromBottom (26).reduced (2, 0));
    r.removeFromBottom (8);

    if (layerModeBox.isVisible())
    {
        auto bottom = r.removeFromBottom (26);
        layerModeBox.setBounds (bottom.reduced (2, 0));
        layersCaption.setBounds (r.removeFromBottom (16));
        r.removeFromBottom (4);
    }

    auto captions = r.removeFromTop (14);
    toneCaption.setBounds (captions.removeFromLeft (captions.getWidth() / 2));
    sendCaption.setBounds (captions);

    toneSlider.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0));
    sendSlider.setBounds (r.reduced (4, 0));
}

} // namespace rollforge
