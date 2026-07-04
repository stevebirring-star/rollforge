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

    on = ! on;
    if (on)
        velocity = velocityForY (e.position.y);
    repaint();

    if (onEdit)
        onEdit (on, velocity);
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
