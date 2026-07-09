#include "ui/PadComponent.h"

#include "ui/Theme.h"

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

    constexpr int trimStripHeight = 12;   // bottom strip height for trim-handle drags
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
    initToggle (muteButton,    theme().textFaint);      // grey  = silenced
    initToggle (soloButton,    theme().accentCool);     // blue  = the machine is listening
    initToggle (reverseButton, theme().accentHot);      // hot   = you changed something
    muteButton.setTooltip ("Mute this pad in the sequencer (click the pad to still audition it)");
    soloButton.setTooltip ("Solo: play only soloed pads");
    reverseButton.setTooltip ("Play this pad's sample backwards");
    muteButton.onClick    = [this] { if (onMute)    onMute    (index, muteButton.getToggleState()); updateControlVisibility(); };
    soloButton.onClick    = [this] { if (onSolo)    onSolo    (index, soloButton.getToggleState()); updateControlVisibility(); };
    reverseButton.onClick = [this] { if (onReverse) onReverse (index, reverseButton.getToggleState()); updateControlVisibility(); };

    // The pad must count "the mouse is over one of my toggles" as being hovered, or the
    // toggles would vanish the instant you reached for one.
    for (auto* b : { &muteButton, &soloButton, &reverseButton })
        b->addMouseListener (this, false);

    updateControlVisibility();
}

void PadComponent::refreshHover()
{
    const bool now = isMouseOver (true);
    if (now != hovered)
    {
        hovered = now;
        updateControlVisibility();
        repaint();
    }
}

void PadComponent::updateControlVisibility()
{
    // Visible when you are reaching for it, or when it is currently doing something.
    muteButton.setVisible    (hovered || muteButton.getToggleState());
    soloButton.setVisible    (hovered || soloButton.getToggleState());
    reverseButton.setVisible (hovered || reverseButton.getToggleState());
}

bool PadComponent::isTrimmed() const noexcept
{
    return trimStart > 0.001f || trimEnd < 0.999f;
}

float PadComponent::trimHandleX (bool end) const noexcept
{
    const auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    return bounds.getX() + (end ? trimEnd : trimStart) * bounds.getWidth();
}

void PadComponent::mouseEnter (const juce::MouseEvent&) { refreshHover(); }
void PadComponent::mouseExit  (const juce::MouseEvent&) { refreshHover(); }

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
    updateControlVisibility();
}

void PadComponent::setSoloed (bool soloed)
{
    soloButton.setToggleState (soloed, juce::dontSendNotification);
    updateControlVisibility();
}

void PadComponent::setAudible (bool shouldBeAudible)
{
    if (audible != shouldBeAudible)
    {
        audible = shouldBeAudible;
        repaint();
    }
}

void PadComponent::setReverse (bool reversed)
{
    reverseButton.setToggleState (reversed, juce::dontSendNotification);
    updateControlVisibility();
}

void PadComponent::setAccent (juce::Colour colour)
{
    if (accent != colour)
    {
        accent = colour;
        repaint();
    }
}

void PadComponent::setTrim (float start, float end)
{
    trimStart = juce::jlimit (0.0f, 1.0f, start);
    trimEnd   = juce::jlimit (0.0f, 1.0f, end);
    repaint();
}

void PadComponent::updateTrimFromX (float x)
{
    constexpr float minGap = 0.03f;
    const float w = (float) juce::jmax (1, getWidth());
    const float f = juce::jlimit (0.0f, 1.0f, x / w);
    if (draggingEnd) trimEnd   = juce::jmax (f, trimStart + minGap);
    else             trimStart = juce::jmin (f, trimEnd   - minGap);
    trimStart = juce::jlimit (0.0f, 1.0f - minGap, trimStart);
    trimEnd   = juce::jlimit (minGap, 1.0f, trimEnd);
    repaint();
    if (onTrim)
        onTrim (index, trimStart, trimEnd);
}

void PadComponent::resized()
{
    // M / S toggles top-left, R top-right. They're child components, so they capture
    // their own clicks and never trigger the pad; the rest of the pad stays clickable.
    auto row = getLocalBounds().reduced (8, 7).removeFromTop (15);
    muteButton.setBounds (row.removeFromLeft (20));
    row.removeFromLeft (3);
    soloButton.setBounds (row.removeFromLeft (20));
    reverseButton.setBounds (row.removeFromRight (20));
}

void PadComponent::paint (juce::Graphics& g)
{
    const auto& t = theme();
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    constexpr float corner = 8.0f;

    const juce::Colour base = t.panelRaised;
    const juce::Colour lit  = accent;

    // A struck pad glows at its own colour, brightening with velocity — the MPC read.
    auto fill = base.interpolatedWith (lit, juce::jlimit (0.0f, 1.0f, flashLevel * 0.55f));
    if (dragOver)
        fill = fill.interpolatedWith (juce::Colours::white, 0.25f);

    g.setColour (t.panelShadow.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), corner);
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.06f), bounds.getCentreX(), bounds.getY(),
                                             fill.darker (0.10f),   bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, corner);

    // The waveform is a calm silhouette, not a comb of 48 bars. It is the pad's texture,
    // not its content; the label has to be able to sit on top of it.
    if (! waveform.empty())
    {
        const float midY = bounds.getCentreY();
        const float maxH = bounds.getHeight() * 0.26f;
        const int   n    = (int) waveform.size();

        juce::Path silhouette;
        silhouette.startNewSubPath (bounds.getX(), midY);
        for (int i = 0; i < n; ++i)
        {
            const float x = bounds.getX() + bounds.getWidth() * ((float) i / (float) (n - 1));
            silhouette.lineTo (x, midY - juce::jlimit (0.0f, 1.0f, waveform[(size_t) i]) * maxH);
        }
        for (int i = n - 1; i >= 0; --i)
        {
            const float x = bounds.getX() + bounds.getWidth() * ((float) i / (float) (n - 1));
            silhouette.lineTo (x, midY + juce::jlimit (0.0f, 1.0f, waveform[(size_t) i]) * maxH);
        }
        silhouette.closeSubPath();

        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (bounds.getSmallestIntegerContainer());
        g.setColour (accent.withAlpha (hovered ? 0.26f : 0.15f));
        g.fillPath (silhouette);
    }

    // Trim: the region outside the play range is dimmed whenever it is trimmed, because
    // that is information. The grab handles only appear when you reach for them.
    if (! waveform.empty() && (isTrimmed() || hovered))
    {
        const float w  = bounds.getWidth();
        const float x0 = bounds.getX();

        if (isTrimmed())
        {
            g.setColour (t.background.withAlpha (0.62f));
            if (trimStart > 0.001f)
                g.fillRect (juce::Rectangle<float> (x0, bounds.getY(), trimStart * w, bounds.getHeight()));
            if (trimEnd < 0.999f)
                g.fillRect (juce::Rectangle<float> (x0 + trimEnd * w, bounds.getY(),
                                                    (1.0f - trimEnd) * w, bounds.getHeight()));
        }

        if (hovered || isTrimmed())
        {
            g.setColour (accent.withAlpha (hovered ? 0.95f : 0.6f));
            const float ty = bounds.getBottom() - 7.0f;
            g.fillRoundedRectangle (trimHandleX (false) - 1.5f, ty, 3.0f, 7.0f, 1.5f);
            g.fillRoundedRectangle (trimHandleX (true)  - 1.5f, ty, 3.0f, 7.0f, 1.5f);
        }
    }

    // The pad's identity ring. Bright while it sounds, quiet at rest — but always its
    // own colour, so the kit is legible in a glance.
    g.setColour (dragOver ? juce::Colours::white
                          : accent.withAlpha (0.32f + 0.62f * juce::jlimit (0.0f, 1.0f, flashLevel)));
    g.drawRoundedRectangle (bounds, corner, 1.5f);
    g.setColour (t.panelHighlight.withAlpha (0.5f));
    g.drawLine (bounds.getX() + corner, bounds.getY() + 0.7f,
                bounds.getRight() - corner, bounds.getY() + 0.7f, 1.0f);

    // The level meter IS the bottom edge of the ring, lit from the left. One fewer object
    // on the tile than a floating track-and-bar, and it reads at a glance across sixteen.
    if (meterLevel > 0.02f)
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, meterLevel);
        const float x0  = bounds.getX() + corner * 0.5f;
        const float x1  = x0 + (bounds.getWidth() - corner) * lvl;
        g.setColour (lvl < 0.9f ? accent : t.meterRedZone);
        g.drawLine (x0, bounds.getBottom() - 1.0f, x1, bounds.getBottom() - 1.0f, 2.4f);
    }

    // The name, given a shadow so it never has to fight the waveform behind it.
    {
        auto text = bounds.reduced (6.0f);
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (t.panelShadow.withAlpha (0.8f));
        g.drawText (label, text.translated (0.0f, 1.0f), juce::Justification::centred, true);
        g.setColour (t.text);
        g.drawText (label, text, juce::Justification::centred, true);
    }

    // Dim the pad when it won't sound under the current mute/solo state. Drawn last so
    // it covers the waveform/label/meter; the M/S child buttons paint on top and stay lit.
    if (! audible)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds, corner);
    }
}

void PadComponent::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent != this)
        return;
    if (trimming)
    {
        trimming = false;
        return;
    }
    if (onRelease)
        onRelease (index);   // ends a held note-repeat
}

void PadComponent::mouseDown (const juce::MouseEvent& e)
{
    // The M/S/R toggles forward their events here so the pad knows it is hovered. They
    // handle their own clicks; the pad must not also fire.
    if (e.eventComponent != this)
        return;

    // Right-click opens the pad's TONE / SEND controls; it never auditions, so the
    // gesture that opens a bubble doesn't also make a noise.
    if (e.mods.isPopupMenu())
    {
        if (onInspect)
            onInspect (index);
        return;
    }

    // A trim drag starts only NEAR a handle. The old rule — anywhere in the bottom strip —
    // meant that clicking the lower third of a pad silently re-trimmed the sample instead
    // of playing it, which is the last thing a pad should do.
    if (! waveform.empty() && e.position.y >= (float) (getHeight() - trimStripHeight))
    {
        constexpr float grabRadius = 9.0f;
        const float dStart = std::abs (e.position.x - trimHandleX (false));
        const float dEnd   = std::abs (e.position.x - trimHandleX (true));

        if (juce::jmin (dStart, dEnd) <= grabRadius)
        {
            draggingEnd = dEnd <= dStart;
            trimming    = true;
            updateTrimFromX (e.position.x);
            return;
        }
    }

    if (onTrigger)
        onTrigger (index, 1.0f);
    flash();
}

void PadComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != this)
        return;
    if (trimming)
        updateTrimFromX (e.position.x);
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

    if (onFilesDropped == nullptr)
        return;

    // Drop one file to load a sample; drop several to stack them as round-robin layers.
    juce::StringArray audioFiles;
    for (const auto& f : files)
        if (isAudioFile (f))
            audioFiles.add (f);

    if (! audioFiles.isEmpty())
        onFilesDropped (index, audioFiles);
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
