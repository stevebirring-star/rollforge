#pragma once

// RollForge — SongBar: the arrangement chain, as a row of chips.
//
// SONG hands the A..H switching over to the chain: "A x2, B, A x2, C". Each chip is one
// step. Click a chip to cycle how many bars it lasts (1, 2, 4, 8); right-click to remove it.
// ADD appends the pattern you are standing in. The chip that is sounding now wears the cool
// accent, the same colour the transport and the playhead wear, because that is the machine
// running rather than the user acting.
//
// The chips are painted, not thirty child components: they are re-laid-out on every edit,
// and a Component each would buy nothing but bookkeeping.
//
// If the chain is longer than the row, the overflow is COUNTED on screen rather than quietly
// clipped — a chain that looks eight steps long but plays twelve is a bug the user cannot see.
//
// UI only: the owner holds the Song and does the editing.

#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class SongBar final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    SongBar();

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Redraw from the owner's chain. */
    void setSong (const Song& song);

    /** Which step is sounding (index into song.steps), or -1. */
    void setPlayingStep (int index);

    /** The letter ADD would append. */
    void setCurrentSlot (int slot);

    void setSongMode (bool on);
    bool isSongMode() const noexcept { return songButton.getToggleState(); }

    std::function<void (bool on)>              onSongModeChanged;
    std::function<void (bool loop)>            onLoopChanged;
    std::function<void()>                      onAppendCurrent;
    std::function<void (int index, int bars)>  onSetBars;
    std::function<void (int index)>            onRemove;
    std::function<void()>                      onClearChain;

private:
    /** Bounds of chip `index`, or an empty rectangle when it did not fit. */
    juce::Rectangle<int> chipBounds (int index) const;
    int chipAt (juce::Point<int> position) const;   // -1 if none
    int visibleChipCount() const;

    /** 1 -> 2 -> 4 -> 8 -> 1. The four lengths a bar-count is ever wanted in. */
    static int nextBars (int bars) noexcept;

    Song song;
    int  playingStep = -1;
    int  hoveredChip = -1;
    int  currentSlot = 0;

    juce::TextButton songButton  { "SONG" };
    juce::TextButton loopButton  { "Loop" };
    juce::TextButton addButton   { "Add A" };
    juce::TextButton clearButton { "Clear" };

    juce::Rectangle<int> chipArea;   // what is left after the buttons

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongBar)
};

} // namespace rollforge
