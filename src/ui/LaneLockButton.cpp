#include "ui/LaneLockButton.h"

namespace rollforge
{

LaneLockButton::LaneLockButton()
{
    setTooltip ("Lock this lane so its steps survive Make a Beat / Reroll");
}

void LaneLockButton::setLocked (bool shouldBeLocked)
{
    if (locked != shouldBeLocked)
    {
        locked = shouldBeLocked;
        repaint();
    }
}

void LaneLockButton::mouseDown (const juce::MouseEvent&)
{
    if (onToggle)
        onToggle();   // the owner flips the authoritative state and calls setLocked back
}

void LaneLockButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // A small centred padlock: locked reads as a bright, filled lock; unlocked as a
    // dim outline (brighter on hover so it's discoverable).
    const juce::Colour on   { 0xff4cc2ff };
    const juce::Colour dim  { 0xff55555f };
    const juce::Colour hint { 0xff8a8a95 };
    const juce::Colour c = locked ? on : (isMouseOver() ? hint : dim);

    constexpr float w = 11.0f;
    constexpr float bodyH = 7.5f;
    const float cx    = b.getCentreX();
    const float bodyY = b.getCentreY() - bodyH * 0.2f;
    const juce::Rectangle<float> body (cx - w * 0.5f, bodyY, w, bodyH);

    // Shackle: an arch of straight legs + a top semicircle sitting on the body.
    const float r = w * 0.30f;
    const float legTop = body.getY() - 1.0f;
    juce::Path shackle;
    shackle.startNewSubPath (cx - r, body.getY() + 0.5f);
    shackle.lineTo (cx - r, legTop);
    shackle.addCentredArc (cx, legTop, r, r, 0.0f,
                           -juce::MathConstants<float>::halfPi,
                            juce::MathConstants<float>::halfPi, false);
    shackle.lineTo (cx + r, body.getY() + 0.5f);

    g.setColour (c);
    g.strokePath (shackle, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    if (locked)
    {
        g.fillRoundedRectangle (body, 1.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));           // keyhole
        g.fillEllipse (cx - 1.0f, body.getCentreY() - 1.4f, 2.0f, 2.0f);
    }
    else
    {
        g.drawRoundedRectangle (body, 1.6f, 1.1f);
    }
}

} // namespace rollforge
