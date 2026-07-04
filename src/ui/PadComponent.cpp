#include "ui/PadComponent.h"

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
}

void PadComponent::setLabelText (const juce::String& text)
{
    label = text;
    repaint();
}

void PadComponent::flash()
{
    flashLevel = 1.0f;
    if (! isTimerRunning())
        startTimerHz (30);
    repaint();
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

    g.setColour (dragOver ? lit : juce::Colour (0xff3a3a44));
    g.drawRoundedRectangle (bounds, corner, 1.5f);

    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (label, bounds.reduced (6.0f), juce::Justification::centred, true);
}

void PadComponent::mouseDown (const juce::MouseEvent&)
{
    if (onTrigger)
        onTrigger (index, 1.0f);
    flash();
}

bool PadComponent::isInterestedInFileDragAndDrop (const juce::StringArray& files)
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
