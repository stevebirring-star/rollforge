#pragma once

// RollForge — PatternSlots: the A..H strip.
//
// Eight patterns, one kit. Click a letter to go there: while the transport runs the change
// lands on the next bar line, so a verse becomes a chorus in time rather than the instant
// your finger moved. The queued letter pulses until it takes; the current one stays lit.
//
// An empty slot is drawn faint, because "which of these have I written?" is the first
// question anyone asks of a bank of eight, and a row of identical buttons cannot answer it.
//
// UI only. The owner does the switching, the copying and the clearing.

#include "model/PatternBank.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>

namespace rollforge
{

class PatternSlots final : public juce::Component,
                           private juce::Timer
{
public:
    PatternSlots();
    ~PatternSlots() override;

    void resized() override;

    /** The slot playing now. */
    void setCurrent (int slot);

    /** The slot waiting for the next bar line, or -1. */
    void setQueued (int slot);

    /** Whether each slot has anything in it, for the faint/solid distinction. */
    void setSlotWritten (int slot, bool written);

    std::function<void (int slot)> onSelect;    // left click
    std::function<void (int slot)> onCopyTo;    // "Copy the current pattern here"
    std::function<void (int slot)> onClear;     // "Clear"

private:
    void timerCallback() override;              // pulses the queued slot
    void showSlotMenu (int slot);

    class SlotButton;

    juce::Label                                   caption;
    std::array<std::unique_ptr<SlotButton>, numPatternSlots> buttons;

    int   current = 0;
    int   queued  = -1;
    float phase   = 0.0f;   // 0..2, ramps up then down
    float pulse   = 0.0f;   // the triangle of `phase`, strictly 0..1 (an alpha)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternSlots)
};

} // namespace rollforge
