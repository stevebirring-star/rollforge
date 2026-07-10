#include "ui/StepComponent.h"

#include "ui/Theme.h"

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

void StepComponent::setAccent (juce::Colour colour)
{
    if (accent != colour)
    {
        accent = colour;
        repaint();
    }
}

void StepComponent::setActive (bool isActive)
{
    if (active != isActive)
    {
        active = isActive;
        setInterceptsMouseClicks (active, false);
        repaint();
    }
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

    // Past the lane's length: a hollow outline, so a triplet lane's four unused columns
    // read as "not part of this row" rather than "an empty step you could turn on".
    const auto& t = theme();

    if (! active)
    {
        g.setColour (t.background.darker (0.25f));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (t.hairline.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
        return;
    }

    // The step wears the colour of the sound it fires. Velocity is then carried by
    // brightness and fill height, not by a second hue — one variable, one channel.
    const juce::Colour off    = t.panel.brighter (0.10f);
    const juce::Colour onLow  = accent.withSaturation (0.45f).darker (0.55f);
    const juce::Colour onHigh = accent;

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
            g.setColour (t.background.withAlpha (0.75f));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (juce::String (pct), bounds, juce::Justification::centred, false);
        }
    }
    else
    {
        g.setColour (off);
        g.fillRoundedRectangle (bounds, corner);
    }

    // The playhead is the machine running: cool, and only ever cool.
    if (current)
    {
        g.setColour (t.accentCool.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (t.accentCool);
        g.drawRoundedRectangle (bounds, corner, 1.6f);
    }
    else
    {
        g.setColour (t.hairline);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    // "Vary just touched this" marker, shown on added hits and cleared cells alike. It is
    // BONE, not accentHot: a kick lane's steps are already hot-orange, and an orange ring
    // on an orange step says nothing. Bone reads against all nine category colours.
    if (changed)
    {
        g.setColour (t.text.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 1.0f, 2.5f);
        g.setColour (t.text);
        g.drawRoundedRectangle (bounds, corner, 2.0f);
    }
}

} // namespace rollforge
