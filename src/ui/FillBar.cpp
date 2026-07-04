#include "ui/FillBar.h"

namespace rollforge
{

FillBar::FillBar (Sequencer& sequencerToUse) : sequencer (sequencerToUse)
{
    for (int i = 0; i < FillEngine::NumStyles; ++i)
        styleBox.addItem (FillEngine::styleName ((FillEngine::Style) i), i + 1);   // itemId is 1-based
    styleBox.setSelectedId (1, juce::dontSendNotification);
    styleBox.setTooltip ("Fill style");
    addAndMakeVisible (styleBox);

    intensitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    intensitySlider.setRange (1.0, 5.0, 1.0);
    intensitySlider.setValue (3.0, juce::dontSendNotification);
    intensitySlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 30, 20);
    intensitySlider.setTooltip ("Fill intensity (1-5)");
    addAndMakeVisible (intensitySlider);

    fillButton.onClick   = [this] { ++seed;      fire(); };
    rerollButton.onClick = [this] { seed += 7ull; fire(); };
    addAndMakeVisible (fillButton);
    addAndMakeVisible (rerollButton);

    humaniseLabel.setText ("Humanise", juce::dontSendNotification);
    humaniseLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (humaniseLabel);

    humaniseSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    humaniseSlider.setRange (0.0, 1.0, 0.01);
    humaniseSlider.setValue (0.0, juce::dontSendNotification);
    humaniseSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    humaniseSlider.setTooltip ("Robot <-> Human feel");
    humaniseSlider.onValueChange = [this] { sequencer.setHumanise ((float) humaniseSlider.getValue()); };
    addAndMakeVisible (humaniseSlider);
}

void FillBar::fire()
{
    if (onFill != nullptr)
    {
        const auto style     = (FillEngine::Style) juce::jlimit (0, FillEngine::NumStyles - 1,
                                                                 styleBox.getSelectedId() - 1);
        const int  intensity = (int) intensitySlider.getValue();
        onFill (style, intensity, seed);
    }
}

void FillBar::resized()
{
    auto r = getLocalBounds();

    styleBox.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (6);
    intensitySlider.setBounds (r.removeFromLeft (130));
    r.removeFromLeft (6);
    fillButton.setBounds (r.removeFromLeft (64));
    r.removeFromLeft (6);
    rerollButton.setBounds (r.removeFromLeft (68));

    // Humanise group on the right.
    humaniseSlider.setBounds (r.removeFromRight (170));
    humaniseLabel.setBounds (r.removeFromRight (74));
}

} // namespace rollforge
