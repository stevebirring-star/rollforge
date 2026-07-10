#include "ui/FirstRun.h"
#include "ui/Theme.h"

namespace rollforge
{

FirstRun::FirstRun()
{
    // The tour is the primary action here, so it wears the hot accent, like every other
    // control in this app that makes something happen.
    tourButton.setColour (juce::TextButton::buttonColourId, theme().accentHot);
    tourButton.setColour (juce::TextButton::textColourOffId, theme().background);
    tourButton.onClick = [this] { if (onTakeTour) onTakeTour(); };
    skipButton.onClick = [this] { if (onSkip)     onSkip(); };
    addAndMakeVisible (tourButton);
    addAndMakeVisible (skipButton);
    setInterceptsMouseClicks (true, true);   // eat clicks to the app underneath
}

void FirstRun::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xcc0e0e12));   // dim the app behind

    auto panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (440, getWidth() - 40),
                                                         juce::jmin (206, getHeight() - 40));
    g.setColour (juce::Colour (0xff26262c));
    g.fillRoundedRectangle (panel.toFloat(), 10.0f);
    g.setColour (theme().accentHot.withAlpha (0.55f));   // teal predated the Forge palette
    g.drawRoundedRectangle (panel.toFloat(), 10.0f, 1.5f);

    auto inner = panel.reduced (22);
    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Welcome to RollForge", inner.removeFromTop (34), juce::Justification::centredLeft, false);

    inner.removeFromTop (10);
    inner.removeFromBottom (34 + 10);   // the button row

    g.setFont (juce::FontOptions (14.0f));
    g.setColour (juce::Colour (0xffbcbcc4));
    g.drawFittedText ("Six short steps, on the app itself, ending with a beat you made. "
                      "You can skip out of it at any point, and reopen it from Help whenever "
                      "you want it back.",
                      inner, juce::Justification::topLeft, 4);
}

void FirstRun::resized()
{
    auto panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (440, getWidth() - 40),
                                                         juce::jmin (206, getHeight() - 40));
    auto row = panel.reduced (22).removeFromBottom (34);

    tourButton.setBounds (row.removeFromRight (140));
    row.removeFromRight (8);
    skipButton.setBounds (row.removeFromRight (100));
}

} // namespace rollforge
