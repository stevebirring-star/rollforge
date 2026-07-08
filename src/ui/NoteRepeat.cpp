#include "ui/NoteRepeat.h"

namespace rollforge
{

const char* NoteRepeat::rateName (Rate rate) noexcept
{
    switch (rate)
    {
        case Eighth:           return "1/8";
        case Sixteenth:        return "1/16";
        case EighthTriplet:    return "1/8T";
        case SixteenthTriplet: return "1/16T";
        case ThirtySecond:     return "1/32";
        case Build:            return "Build";
        default:               return "1/16";
    }
}

void NoteRepeat::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled = shouldBeEnabled;
    if (! enabled && activePad >= 0)
    {
        activePad = -1;
        stopTimer();
    }
}

void NoteRepeat::setRate (Rate r) noexcept
{
    rate = r;
    // If a pad is being held right now, retime the repeat immediately so a mid-hold
    // rate change takes effect at once rather than only on the next press.
    if (enabled && activePad >= 0)
        startTimer (intervalMs());
}

void NoteRepeat::noteOn (int pad, float velocity, double currentBpm)
{
    if (! enabled || pad < 0)
        return;

    activePad = pad;
    baseVel   = velocity;
    bpm       = currentBpm;
    hitCount  = 0;
    startTimer (intervalMs());   // the caller already fired hit #1 on mouse-down
}

void NoteRepeat::noteOff (int pad)
{
    if (pad == activePad)
    {
        activePad = -1;
        stopTimer();
    }
}

void NoteRepeat::timerCallback()
{
    if (activePad < 0)
    {
        stopTimer();
        return;
    }

    ++hitCount;
    if (onHit)
        onHit (activePad, velocityForHit());

    // Build accelerates, so re-arm with the new (shorter) interval each hit; fixed
    // rates keep the timer's constant period.
    if (rate == Build)
        startTimer (intervalMs());
}

float NoteRepeat::velocityForHit() const noexcept
{
    if (rate == Build)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (float) hitCount / 16.0f);
        return juce::jlimit (0.15f, 1.0f, 0.55f + 0.5f * t);   // crescendo into the roll
    }
    return baseVel;
}

int NoteRepeat::intervalMs() const noexcept
{
    const double beat = 60000.0 / (bpm > 1.0 ? bpm : 120.0);   // ms per quarter note
    double div;
    switch (rate)
    {
        case Eighth:           div = 2.0; break;
        case Sixteenth:        div = 4.0; break;
        case EighthTriplet:    div = 3.0; break;
        case SixteenthTriplet: div = 6.0; break;
        case ThirtySecond:     div = 8.0; break;
        case Build:
        {
            const double t = juce::jlimit (0.0, 1.0, (double) hitCount / 16.0);
            div = 4.0 + 4.0 * t;   // 1/16 -> 1/32 over ~16 hits
            break;
        }
        default: div = 4.0; break;
    }
    return juce::jmax (20, juce::roundToInt (beat / div));   // clamp to a sane minimum
}

} // namespace rollforge
