#include "ui/StepComponent.h"

namespace rollforge
{

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

    // Turning a step ON lights it and sets velocity from the click height now, so a
    // continued drag adjusts it. An already-on step waits: a drag adjusts its
    // velocity (below), while a plain click toggles it off in mouseUp — so you can
    // fine-tune a live step's level without having to switch it off first.
    if (! on)
    {
        on = true;
        velocity = velocityForY (e.position.y);
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
}

} // namespace rollforge
