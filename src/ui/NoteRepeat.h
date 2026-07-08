#pragma once

// RollForge — NoteRepeat: hold a pad to retrigger it at a chosen rate, BPM-synced.
// A finger-driven live twin of the RollCompiler. The initial hit is the caller's
// own pad trigger (mouse-down); while the pad is held and Repeat is enabled, this
// fires the SUBSEQUENT hits via onHit on the message thread.
//
// "Build" is the differentiator vs a flat note-repeat (Maschine): it accelerates
// 1/16 -> 1/32 and crescendos the velocity while held — a live roll build-up.
// UI/message-thread only (owns a juce::Timer).

#include <juce_events/juce_events.h>

#include <functional>

namespace rollforge
{

class NoteRepeat final : private juce::Timer
{
public:
    NoteRepeat() = default;

    enum Rate { Eighth = 0, Sixteenth, EighthTriplet, SixteenthTriplet, ThirtySecond, Build, NumRates };

    static const char* rateName (Rate rate) noexcept;

    void setEnabled (bool shouldBeEnabled) noexcept;   // turning off stops any active repeat
    bool isEnabled() const noexcept { return enabled; }
    void setRate (Rate r) noexcept { rate = r; }

    /** Pad pressed: begin repeating (only if enabled). `bpm` is the current tempo. */
    void noteOn (int pad, float velocity, double currentBpm);
    /** Pad released: stop repeating that pad. */
    void noteOff (int pad);

    /** Fires each repeat hit — the owner routes it to the engine (+ pad flash). */
    std::function<void (int pad, float velocity)> onHit;

private:
    void timerCallback() override;
    int  intervalMs() const noexcept;
    float velocityForHit() const noexcept;

    bool   enabled   = false;
    Rate   rate      = Sixteenth;
    int    activePad = -1;
    float  baseVel   = 1.0f;
    double bpm       = 120.0;
    int    hitCount  = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteRepeat)
};

} // namespace rollforge
