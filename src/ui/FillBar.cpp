#include "ui/FillBar.h"

#include "ui/RollForgeLookAndFeel.h"
#include "ui/Theme.h"

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
    // Make a Beat is the app's loudest promise, so it wears the hot accent. Everything
    // the user MAKES is orange; everything the machine DOES is blue.
    fillButton.setColour (juce::TextButton::buttonColourId, theme().accentHot);
    fillButton.setColour (juce::TextButton::textColourOffId, theme().background);
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

    // Reroll changes the notes and keeps the sounds; New Sounds does exactly the opposite.
    // The pair is the point: a groove you like is worth auditioning against a dozen kits.
    soundsButton.onClick = [this] { seed += 29ull; if (onRerollSounds) onRerollSounds (seed); };
    soundsButton.setTooltip ("Swap the kit's samples from your library, keep the groove exactly as it is");
    addAndMakeVisible (soundsButton);

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

void FillBar::setLibraryAvailable (bool available)
{
    soundsButton.setEnabled (available);
    soundsButton.setTooltip (available
        ? "Swap the kit's samples from your library, keep the groove exactly as it is"
        : "Scan a samples folder in the Library first, then this rerolls the kit");
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

    // Widths are tuned so the whole row still fits at the 780 px minimum window width.
    fillButton.setBounds (r.removeFromLeft (120));     // primary action, leads the row
    r.removeFromLeft (8);
    rerollButton.setBounds (r.removeFromLeft (58));
    r.removeFromLeft (6);
    varyButton.setBounds (r.removeFromLeft (58));
    r.removeFromLeft (6);
    soundsButton.setBounds (r.removeFromLeft (84));    // sits with its mirror, Reroll
    r.removeFromLeft (12);
    styleBox.setBounds (r.removeFromLeft (90));
    r.removeFromLeft (6);
    intensitySlider.setBounds (r.removeFromLeft (96));

    // Feel group on the right.
    feelBox.setBounds (r.removeFromRight (150));
    feelLabel.setBounds (r.removeFromRight (34));
}

} // namespace rollforge
