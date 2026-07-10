#include "ui/SongBar.h"

#include "ui/Theme.h"

namespace rollforge
{

namespace
{
    constexpr int chipWidth = 44;
    constexpr int chipGap   = 4;

    juce::String slotLetter (int slot)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + slot));
    }
}

SongBar::SongBar()
{
    songButton.setClickingTogglesState (true);
    songButton.setTooltip ("Play the chain instead of one pattern. Each step switches on its bar line.");
    songButton.setColour (juce::TextButton::buttonOnColourId, theme().accentHot);
    songButton.setColour (juce::TextButton::textColourOnId,   theme().background);
    songButton.onClick = [this]
    {
        if (onSongModeChanged != nullptr)
            onSongModeChanged (songButton.getToggleState());
    };
    addAndMakeVisible (songButton);

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (true, juce::dontSendNotification);
    loopButton.setTooltip ("Repeat the chain forever. Off: the transport stops when it ends.");

    // SONG arms the arrangement, so it wears the hot accent. Loop is a setting on that
    // arrangement, not a second action, and must not shout as loudly: it lights in ember,
    // the colour this app gives to a value rather than to an intent.
    loopButton.setColour (juce::TextButton::buttonOnColourId, theme().panelRaised.brighter (0.3f));
    loopButton.setColour (juce::TextButton::textColourOnId,   theme().ember);
    loopButton.onClick = [this]
    {
        if (onLoopChanged != nullptr)
            onLoopChanged (loopButton.getToggleState());
    };
    addAndMakeVisible (loopButton);

    addButton.setTooltip ("Append the pattern you are standing in to the end of the chain");
    addButton.onClick = [this] { if (onAppendCurrent) onAppendCurrent(); };
    addAndMakeVisible (addButton);

    // Named for what it empties. "Clear" alone, sitting a row under the pattern keys,
    // reads as "clear the beat" — and then does nothing visible, because the chain it
    // really clears is usually already empty. It greys out when there is nothing to clear.
    clearButton.setTooltip ("Empty the song chain. This does not touch the beat.");
    clearButton.setEnabled (false);
    clearButton.onClick = [this] { if (onClearChain) onClearChain(); };
    addAndMakeVisible (clearButton);

    setTooltip ("Click a step to cycle its length (1, 2, 4, 8 bars). Right-click to remove it.");
}

void SongBar::setSong (const Song& s)
{
    song = s;
    loopButton.setToggleState (song.loop, juce::dontSendNotification);
    clearButton.setEnabled (! song.steps.empty());
    repaint();
}

void SongBar::setPlayingStep (int index)
{
    if (playingStep == index)
        return;
    playingStep = index;
    repaint();
}

void SongBar::setCurrentSlot (int slot)
{
    if (! PatternBank::isValidSlot (slot) || currentSlot == slot)
        return;
    currentSlot = slot;
    addButton.setButtonText ("Add " + slotLetter (slot));
}

void SongBar::setSongMode (bool on)
{
    songButton.setToggleState (on, juce::dontSendNotification);
    repaint();
}

int SongBar::nextBars (int bars) noexcept
{
    switch (bars)
    {
        case 1:  return 2;
        case 2:  return 4;
        case 4:  return 8;
        default: return 1;
    }
}

void SongBar::resized()
{
    auto r = getLocalBounds();
    songButton.setBounds (r.removeFromLeft (60));
    r.removeFromLeft (6);
    loopButton.setBounds (r.removeFromLeft (52));
    r.removeFromLeft (6);
    addButton.setBounds (r.removeFromLeft (66));
    r.removeFromLeft (6);
    clearButton.setBounds (r.removeFromLeft (86));
    r.removeFromLeft (10);
    chipArea = r;
}

int SongBar::visibleChipCount() const
{
    if (chipArea.getWidth() <= 0)
        return 0;

    // One slot at the end is kept back for the "+N more" marker whenever the chain overflows,
    // so the count on screen always adds up to the chain that actually plays.
    const int fits = (chipArea.getWidth() + chipGap) / (chipWidth + chipGap);
    const int total = (int) song.steps.size();
    return total <= fits ? total : juce::jmax (0, fits - 1);
}

juce::Rectangle<int> SongBar::chipBounds (int index) const
{
    if (index < 0 || index >= visibleChipCount())
        return {};

    return { chipArea.getX() + index * (chipWidth + chipGap), chipArea.getY(),
             chipWidth, chipArea.getHeight() };
}

int SongBar::chipAt (juce::Point<int> position) const
{
    for (int i = 0; i < visibleChipCount(); ++i)
        if (chipBounds (i).contains (position))
            return i;
    return -1;
}

void SongBar::mouseMove (const juce::MouseEvent& e)
{
    const int chip = chipAt (e.getPosition());
    if (chip != hoveredChip)
    {
        hoveredChip = chip;
        repaint();
    }
}

void SongBar::mouseExit (const juce::MouseEvent&)
{
    if (hoveredChip >= 0)
    {
        hoveredChip = -1;
        repaint();
    }
}

void SongBar::mouseDown (const juce::MouseEvent& e)
{
    const int chip = chipAt (e.getPosition());
    if (chip < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        if (onRemove != nullptr)
            onRemove (chip);
    }
    else if (onSetBars != nullptr)
    {
        onSetBars (chip, nextBars (song.steps[(std::size_t) chip].bars));
    }
}

void SongBar::paint (juce::Graphics& g)
{
    const auto& t = theme();

    if (song.steps.empty())
    {
        g.setColour (t.textFaint);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Add patterns to build an arrangement", chipArea,
                    juce::Justification::centredLeft);
        return;
    }

    const int visible = visibleChipCount();
    for (int i = 0; i < visible; ++i)
    {
        const auto  b       = chipBounds (i).toFloat().reduced (0.0f, 2.0f);
        const auto& step    = song.steps[(std::size_t) i];
        const bool  playing = (i == playingStep);

        g.setColour (playing ? t.accentCool.withAlpha (0.25f)
                             : (i == hoveredChip ? t.panelRaised.brighter (0.25f) : t.panelRaised));
        g.fillRoundedRectangle (b, 3.0f);

        // The sounding step wears the cool accent: the machine running, not the user acting.
        g.setColour (playing ? t.accentCool : t.hairline);
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, playing ? 1.4f : 1.0f);

        const int bars = juce::jmax (1, step.bars);
        const auto label = PatternBank::isValidSlot (step.slot)
                               ? slotLetter (step.slot) + (bars > 1 ? " x" + juce::String (bars) : juce::String())
                               : juce::String ("?");

        g.setColour (playing ? t.text : t.textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (label, chipBounds (i), juce::Justification::centred);
    }

    // Never clip in silence: say how many steps are playing that you cannot see.
    const int hidden = (int) song.steps.size() - visible;
    if (hidden > 0)
    {
        auto rest = chipArea.withTrimmedLeft (visible * (chipWidth + chipGap));
        g.setColour (t.textFaint);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("+" + juce::String (hidden), rest, juce::Justification::centredLeft);
    }
}

} // namespace rollforge
