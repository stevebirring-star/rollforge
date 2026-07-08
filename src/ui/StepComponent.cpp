#include "ui/StepComponent.h"

namespace rollforge
{

namespace
{
    // A fresh step enable uses this fixed level (matches the model / grid default),
    // so a plain "turn it on" click gives a predictable velocity; a drag afterwards
    // fine-tunes it. Deriving the level from the click-Y on enable made a click land
    // anywhere from ~5% to ~95% on the short 16-lane rows.
    constexpr float defaultOnVelocity = 0.8f;
}

void StepComponent::setState (bool isOn, float vel)
{
    on = isOn;
    velocity = juce::jlimit (0.05f, 1.0f, vel);
    repaint();
}

void StepComponent::setPlayhead (bool isCurrent)
{
    if (current != isCurrent)
    {
        current = isCurrent;
        repaint();
    }
}

void StepComponent::setChanged (bool wasJustChanged)
{
    if (changed != wasJustChanged)
    {
        changed = wasJustChanged;
        repaint();
    }
}

float StepComponent::velocityForY (float y) const noexcept
{
    const float h = (float) juce::jmax (1, getHeight());
    return juce::jlimit (0.05f, 1.0f, 1.0f - y / h);   // top = loud
}

void StepComponent::mouseDown (const juce::MouseEvent& e)
{
    if (onGestureStart)
        onGestureStart();

    editing   = true;
    downWasOn = on;

    // Turning a step ON lights it at a consistent default level; a continued drag
    // then adjusts its velocity from the pointer height (see mouseDrag). An already-on
    // step waits: a drag adjusts its velocity (below), while a plain click toggles it
    // off in mouseUp — so you can fine-tune a live step's level without switching it off.
    if (! on)
    {
        on = true;
        velocity = defaultOnVelocity;
        if (onEdit)
            onEdit (on, velocity);
    }
    repaint();
}

void StepComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! on)
        return;

    velocity = velocityForY (e.position.y);
    repaint();

    if (onEdit)
        onEdit (on, velocity);
}

void StepComponent::mouseUp (const juce::MouseEvent& e)
{
    // A plain click on an already-on step turns it off; a drag was a velocity edit.
    if (downWasOn && ! e.mouseWasDraggedSinceMouseDown())
    {
        on = false;
        if (onEdit)
            onEdit (on, velocity);
    }

    editing = false;
    repaint();
}

void StepComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.5f);
    constexpr float corner = 3.0f;

    const juce::Colour off    { 0xff23232a };
    const juce::Colour onLow  { 0xff2f5d73 };
    const juce::Colour onHigh { 0xff4cc2ff };

    if (on)
    {
        g.setColour (off);
        g.fillRoundedRectangle (bounds, corner);

        // Velocity as a bottom-anchored fill, coloured low->high.
        auto fill = bounds.withTrimmedTop (bounds.getHeight() * (1.0f - velocity));
        g.setColour (onLow.interpolatedWith (onHigh, velocity));
        g.fillRoundedRectangle (fill, corner);

        // Percentage readout so you can gauge how much of the sound this step
        // applies. Prominent (with a dark chip) while you're setting it; a subtle
        // number the rest of the time.
        const int pct = juce::roundToInt (velocity * 100.0f);
        if (editing)
        {
            auto chip = bounds.withSizeKeepingCentre (bounds.getWidth() * 0.86f, 15.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (chip, 3.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (juce::String (pct) + "%", bounds, juce::Justification::centred, false);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.72f));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (juce::String (pct), bounds, juce::Justification::centred, false);
        }
    }
    else
    {
        g.setColour (off);
        g.fillRoundedRectangle (bounds, corner);
    }

    g.setColour (current ? juce::Colours::white
                         : juce::Colour (0xff3a3a44));
    g.drawRoundedRectangle (bounds, corner, current ? 2.0f : 1.0f);

    // "Vary just touched this" marker: an amber ring over the normal border, shown
    // on both added hits and cleared cells (so you can see what was removed too).
    if (changed)
    {
        g.setColour (juce::Colour (0xffffb43a));
        g.drawRoundedRectangle (bounds, corner, 2.0f);
    }
}

} // namespace rollforge
