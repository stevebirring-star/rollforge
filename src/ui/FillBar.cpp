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

    // "Make a Beat" is the flagship one-tap: generate a full genre groove (kick /
    // snare / hats / toms + a roll) with the current style + intensity, a fresh
    // variation each press. Styled as the primary accent action; Reroll just
    // re-rolls the same settings.
    fillButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff4cc2ff));
    fillButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    fillButton.onClick   = [this] { ++seed;       fire(); };
    rerollButton.onClick = [this] { seed += 7ull;  fire(); };
    rerollButton.setTooltip ("Generate a fresh beat from the current style + intensity");
    addAndMakeVisible (fillButton);
    addAndMakeVisible (rerollButton);

    // Vary evolves the CURRENT beat (a few hits on/off, ghost notes, accents) and
    // highlights what changed — the verse/chorus/fill move, not a fresh generate.
    varyButton.onClick = [this] { seed += 13ull; fireVary(); };
    varyButton.setTooltip ("Tweak the current beat: nudge a few hits, add ghosts, vary accents");
    addAndMakeVisible (varyButton);

    // Feel: the one-knob Humaniser exposed as named grooves. Each preset sets the
    // Humaniser amount AND swing together, so a beat never sounds quantized-robotic.
    feelLabel.setText ("Feel", juce::dontSendNotification);
    feelLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (feelLabel);

    for (int i = 0; i < FeelPresets::NumFeels; ++i)
        feelBox.addItem (FeelPresets::name ((FeelPresets::Feel) i), i + 1);   // itemId is 1-based
    feelBox.setSelectedId (FeelPresets::Straight + 1, juce::dontSendNotification);
    feelBox.setTooltip ("Groove feel: sets humanise + swing together");
    feelBox.onChange = [this]
    {
        applyFeel ((FeelPresets::Feel) juce::jlimit (0, FeelPresets::NumFeels - 1,
                                                     feelBox.getSelectedId() - 1));
    };
    addAndMakeVisible (feelBox);
}

void FillBar::applyFeel (FeelPresets::Feel feel)
{
    const auto s = FeelPresets::settingsFor (feel);
    sequencer.setHumanise (s.humanise);
    if (onFeelSwing != nullptr)
        onFeelSwing (s.swing);   // owner reflects it onto the transport's swing control
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

void FillBar::fireVary()
{
    if (onVary != nullptr)
        onVary ((int) intensitySlider.getValue(), seed);
}

void FillBar::resized()
{
    auto r = getLocalBounds();

    fillButton.setBounds (r.removeFromLeft (120));     // primary action, leads the row
    r.removeFromLeft (8);
    rerollButton.setBounds (r.removeFromLeft (58));
    r.removeFromLeft (6);
    varyButton.setBounds (r.removeFromLeft (58));
    r.removeFromLeft (12);
    styleBox.setBounds (r.removeFromLeft (96));
    r.removeFromLeft (6);
    intensitySlider.setBounds (r.removeFromLeft (108));

    // Feel group on the right.
    feelBox.setBounds (r.removeFromRight (150));
    feelLabel.setBounds (r.removeFromRight (42));
}

} // namespace rollforge
