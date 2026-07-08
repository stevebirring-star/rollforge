#include "ui/PadComponent.h"

#include <cmath>

namespace rollforge
{

namespace
{
    bool isAudioFile (const juce::String& path)
    {
        const auto ext = juce::File (path).getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff"
            || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
    }
}

PadComponent::PadComponent (int padIndex)
    : index (padIndex)
{
    setWantsKeyboardFocus (false);

    auto initToggle = [this] (juce::TextButton& b, juce::Colour onColour)
    {
        b.setClickingTogglesState (true);
        b.setWantsKeyboardFocus (false);   // never steal focus from the app's key handler
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff26262c));
        b.setColour (juce::TextButton::buttonOnColourId, onColour);
        b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9a9aa4));
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        addAndMakeVisible (b);
    };
    initToggle (muteButton, juce::Colour (0xffe0553f));   // red   = muted
    initToggle (soloButton, juce::Colour (0xffe0c341));   // amber = soloed
    muteButton.setTooltip ("Mute this pad in the sequencer (click the pad to still audition it)");
    soloButton.setTooltip ("Solo: play only soloed pads");
    muteButton.onClick = [this] { if (onMute) onMute (index, muteButton.getToggleState()); };
    soloButton.onClick = [this] { if (onSolo) onSolo (index, soloButton.getToggleState()); };
}

void PadComponent::setLabelText (const juce::String& text)
{
    label = text;
    repaint();
}

void PadComponent::setMeter (float level)
{
    // Fast attack (jump straight up), slow release (ease down) -> classic VU feel.
    // Driven by the app's 30 Hz UI timer, which calls this for every pad each tick.
    const float target = juce::jmax (juce::jlimit (0.0f, 1.0f, level), meterLevel * 0.80f);
    if (std::abs (target - meterLevel) > 0.002f)
    {
        meterLevel = target;
        repaint();
    }
}

void PadComponent::setWaveform (const std::vector<float>& peaks)
{
    waveform = peaks;
    repaint();
}

void PadComponent::flash()
{
    flashLevel = 1.0f;
    if (! isTimerRunning())
        startTimerHz (30);
    repaint();
}

void PadComponent::setMuted (bool muted)
{
    muteButton.setToggleState (muted, juce::dontSendNotification);
}

void PadComponent::setSoloed (bool soloed)
{
    soloButton.setToggleState (soloed, juce::dontSendNotification);
}

void PadComponent::setAudible (bool shouldBeAudible)
{
    if (audible != shouldBeAudible)
    {
        audible = shouldBeAudible;
        repaint();
    }
}

void PadComponent::resized()
{
    // Small M / S toggles in the top-left. They're child components, so they capture
    // their own clicks and never trigger the pad; the rest of the pad stays clickable.
    auto row = getLocalBounds().reduced (8, 7).removeFromTop (15);
    muteButton.setBounds (row.removeFromLeft (20));
    row.removeFromLeft (3);
    soloButton.setBounds (row.removeFromLeft (20));
}

void PadComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    constexpr float corner = 8.0f;

    const juce::Colour base { 0xff2a2a31 };
    const juce::Colour lit  { 0xff4cc2ff };

    auto fill = base.interpolatedWith (lit, juce::jlimit (0.0f, 1.0f, flashLevel * 0.8f));
    if (dragOver)
        fill = fill.interpolatedWith (juce::Colours::white, 0.25f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, corner);

    // Waveform thumbnail: mirrored around the vertical centre, behind the label.
    if (! waveform.empty())
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (bounds.getSmallestIntegerContainer());

        const float midY = bounds.getCentreY();
        const float maxH = bounds.getHeight() * 0.32f;
        const int   n    = (int) waveform.size();
        const float step = bounds.getWidth() / (float) n;

        g.setColour (lit.withAlpha (0.30f));
        for (int i = 0; i < n; ++i)
        {
            const float h = juce::jlimit (0.0f, 1.0f, waveform[(size_t) i]) * maxH;
            const float x = bounds.getX() + (float) i * step;
            g.fillRect (juce::Rectangle<float> (x, midY - h, juce::jmax (1.0f, step - 1.0f), 2.0f * h));
        }
    }

    g.setColour (dragOver ? lit : juce::Colour (0xff3a3a44));
    g.drawRoundedRectangle (bounds, corner, 1.5f);

    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (label, bounds.reduced (6.0f), juce::Justification::centred, true);

    // Level meter: a bar along the bottom edge, width ∝ output level, green→red.
    if (meterLevel > 0.01f)
    {
        auto track = bounds.reduced (8.0f).removeFromBottom (4.0f);
        g.setColour (juce::Colour (0xff141418));
        g.fillRoundedRectangle (track, 2.0f);

        const float lvl = juce::jlimit (0.0f, 1.0f, meterLevel);
        const juce::Colour barCol = lvl < 0.7f ? juce::Colour (0xff45d17a)
                                  : lvl < 0.9f ? juce::Colour (0xffe0c341)
                                               : juce::Colour (0xffe0553f);
        g.setColour (barCol);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * lvl), 2.0f);
    }

    // Dim the pad when it won't sound under the current mute/solo state. Drawn last so
    // it covers the waveform/label/meter; the M/S child buttons paint on top and stay lit.
    if (! audible)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds, corner);
    }
}

void PadComponent::mouseUp (const juce::MouseEvent&)
{
    if (onRelease)
        onRelease (index);   // ends a held note-repeat
}

void PadComponent::mouseDown (const juce::MouseEvent&)
{
    if (onTrigger)
        onTrigger (index, 1.0f);
    flash();
}

bool PadComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (f))
            return true;
    return false;
}

void PadComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    dragOver = true;
    repaint();
}

void PadComponent::fileDragExit (const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void PadComponent::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();

    if (onFileDropped)
        for (const auto& f : files)
            if (isAudioFile (f))
            {
                onFileDropped (index, juce::File (f));
                break;   // one sample per pad
            }
}

void PadComponent::timerCallback()
{
    flashLevel *= 0.85f;
    if (flashLevel < 0.02f)
    {
        flashLevel = 0.0f;
        stopTimer();
    }
    repaint();
}

} // namespace rollforge
