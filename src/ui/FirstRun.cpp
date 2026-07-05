#include "ui/FirstRun.h"

namespace rollforge
{

FirstRun::FirstRun()
{
    startButton.onClick = [this] { if (onDismissed) onDismissed(); };
    addAndMakeVisible (startButton);
    setInterceptsMouseClicks (true, true);   // eat clicks to the app underneath
}

void FirstRun::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xcc0e0e12));   // dim the app behind

    auto panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (440, getWidth() - 40),
                                                         juce::jmin (300, getHeight() - 40));
    g.setColour (juce::Colour (0xff26262c));
    g.fillRoundedRectangle (panel.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff2a7a74));
    g.drawRoundedRectangle (panel.toFloat(), 10.0f, 1.5f);

    auto inner = panel.reduced (22);
    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Welcome to RollForge", inner.removeFromTop (34), juce::Justification::centredLeft, false);

    inner.removeFromTop (8);
    g.setFont (juce::FontOptions (14.0f));
    g.setColour (juce::Colour (0xffbcbcc4));
    const char* tips[] = {
        "•  Press Play (or the transport) to hear the demo beat.",
        "•  Click the step grid to program pads; drag up for velocity.",
        "•  Turn on Roll Brush and drag a lane to paint an accelerating roll.",
        "•  FILL makes a drum fill; the macro knobs shape the master sound.",
        "•  Library scans a folder + NEW KIT builds a kit; Export saves MIDI/WAV."
    };
    for (auto* t : tips)
        g.drawText (t, inner.removeFromTop (26), juce::Justification::centredLeft, true);
}

void FirstRun::resized()
{
    auto panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (440, getWidth() - 40),
                                                         juce::jmin (300, getHeight() - 40));
    startButton.setBounds (panel.reduced (22).removeFromBottom (34).removeFromRight (120));
}

} // namespace rollforge
