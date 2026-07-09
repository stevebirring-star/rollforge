#pragma once

// RollForge — ToneFilter: a one-knob bipolar tilt EQ, per VOICE.
//
// tone < 0 tilts dark, tone > 0 tilts bright, and tone == 0 is an EXACT bypass (the
// filter is skipped entirely, not run with unity coefficients). It is a low shelf and
// a high shelf about the same 700 Hz pivot, moved in opposite directions — so a drum
// keeps its level while its balance moves, which is the useful thing to have per pad.
//
// It lives on the Voice, not on a per-pad bus: pads have no sub-mix, and giving each
// one a bus would mean 16 real-time scratch buffers to gain one filter. Single-channel
// state, because the Voice filters its mono source before panning.
//
// The RBJ shelf coefficients match engine/fx/MasterEq.cpp. They are deliberately a copy:
// MasterEq's helpers are private, bound to its 8-channel Biquad, and shipped + tested.
//
// RT-SAFE: setTone() runs per note-on (a few transcendentals, and only when tone != 0);
// processSample() is branch-free arithmetic. Neither allocates.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

namespace rollforge
{

class ToneFilter
{
public:
    /** Recomputes coefficients for `tone` in [-1, +1]. Also resets the filter state,
        since a Voice is a one-shot and each note starts clean. */
    void setTone (double sampleRate, float tone) noexcept;

    /** Clears the filter state without touching the coefficients. */
    void reset() noexcept;

    /** False when tone == 0 — the caller should skip filtering entirely. */
    bool isActive() const noexcept { return active; }

    float processSample (float x) noexcept
    {
        return high.process (low.process (x));
    }

    /** Peak tilt at |tone| == 1, in dB, applied to each shelf in opposite directions. */
    static constexpr float maxTiltDb = 9.0f;

private:
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        float process (float x) noexcept
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void resetState() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
    };

    static void setLowShelf  (Biquad&, double sampleRate, double fc, double gainDb) noexcept;
    static void setHighShelf (Biquad&, double sampleRate, double fc, double gainDb) noexcept;

    Biquad low, high;
    bool   active = false;
};

} // namespace rollforge
