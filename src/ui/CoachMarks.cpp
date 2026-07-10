#include "ui/CoachMarks.h"

#include "ui/Theme.h"

namespace rollforge
{

void CoachMarks::setSteps (std::vector<Step> newSteps)
{
    steps = std::move (newSteps);

    setWantsKeyboardFocus (true);

    nextButton.onClick = [this] { goToStep (current + 1); };
    skipButton.onClick = [this] { finish(); };

    nextButton.setColour (juce::TextButton::buttonColourId, theme().accentHot);
    nextButton.setColour (juce::TextButton::textColourOffId, theme().background);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (skipButton);

    goToStep (0);
}

void CoachMarks::goToStep (int index)
{
    if (index >= (int) steps.size())
    {
        finish();
        return;
    }

    current = juce::jmax (0, index);
    nextButton.setButtonText (current == (int) steps.size() - 1 ? "Done" : "Next");
    resized();
    repaint();
    grabKeyboardFocus();
}

void CoachMarks::finish()
{
    if (onFinished != nullptr)
        onFinished();
}

bool CoachMarks::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::escapeKey))
    {
        finish();
        return true;
    }
    if (key.isKeyCode (juce::KeyPress::returnKey) || key.isKeyCode (juce::KeyPress::spaceKey))
    {
        goToStep (current + 1);
        return true;
    }
    return false;
}

juce::Rectangle<int> CoachMarks::holeBounds() const
{
    if (current >= (int) steps.size())
        return {};

    auto* target = steps[(std::size_t) current].target;
    if (target == nullptr || ! target->isShowing())
        return {};

    return getLocalArea (target, target->getLocalBounds()).expanded (holePadding);
}

juce::Rectangle<int> CoachMarks::bubbleBounds() const
{
    const auto hole = holeBounds();
    const auto area = getLocalBounds();

    if (hole.isEmpty())
        return area.withSizeKeepingCentre (bubbleWidth, bubbleHeight);

    // Below the hole if it fits, above if not, and beside it when the hole is tall enough to
    // leave no room either way. Whatever happens, the bubble must not overlap the hole: the
    // hole is a dead zone for hit-testing, and the Next button lives in the bubble.
    juce::Rectangle<int> bubble (0, 0, bubbleWidth, bubbleHeight);

    if (hole.getBottom() + gap + bubbleHeight <= area.getBottom())
        bubble.setPosition (hole.getCentreX() - bubbleWidth / 2, hole.getBottom() + gap);
    else if (hole.getY() - gap - bubbleHeight >= area.getY())
        bubble.setPosition (hole.getCentreX() - bubbleWidth / 2, hole.getY() - gap - bubbleHeight);
    else if (hole.getRight() + gap + bubbleWidth <= area.getRight())
        bubble.setPosition (hole.getRight() + gap, hole.getCentreY() - bubbleHeight / 2);
    else
        bubble.setPosition (hole.getX() - gap - bubbleWidth, hole.getCentreY() - bubbleHeight / 2);

    return bubble.constrainedWithin (area.reduced (8));
}

bool CoachMarks::hitTest (int x, int y)
{
    // Clicks inside the hole belong to the control it exposes. Everything else is ours: the
    // dim, the bubble, and the two buttons.
    return ! holeBounds().contains (x, y);
}

void CoachMarks::resized()
{
    auto bubble = bubbleBounds().reduced (14);
    auto row    = bubble.removeFromBottom (28);

    nextButton.setBounds (row.removeFromRight (78));
    row.removeFromRight (8);
    skipButton.setBounds (row.removeFromRight (86));
}

void CoachMarks::paint (juce::Graphics& g)
{
    if (current >= (int) steps.size())
        return;

    const auto& t    = theme();
    const auto  hole = holeBounds();
    const auto  area = getLocalBounds();

    // Dim everything except the hole. An even-odd path over two rectangles is the whole trick.
    juce::Path shade;
    shade.setUsingNonZeroWinding (false);
    shade.addRectangle (area.toFloat());
    if (! hole.isEmpty())
        shade.addRoundedRectangle (hole.toFloat(), 5.0f);

    g.setColour (juce::Colour (0xcc0b0d12));
    g.fillPath (shade);

    if (! hole.isEmpty())
    {
        g.setColour (t.accentHot);
        g.drawRoundedRectangle (hole.toFloat(), 5.0f, 2.0f);
    }

    const auto bubble = bubbleBounds();
    g.setColour (t.panelRaised);
    g.fillRoundedRectangle (bubble.toFloat(), 7.0f);
    g.setColour (t.accentHot.withAlpha (0.55f));
    g.drawRoundedRectangle (bubble.toFloat(), 7.0f, 1.2f);

    auto inner = bubble.reduced (14);

    // "2 of 5" sits above the title, so someone who hates tours can see how much is left.
    g.setColour (t.textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (juce::String (current + 1) + " of " + juce::String ((int) steps.size()),
                inner.removeFromTop (13), juce::Justification::centredLeft);

    g.setColour (t.text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (steps[(std::size_t) current].title, inner.removeFromTop (21),
                juce::Justification::centredLeft);

    inner.removeFromBottom (28 + 6);   // the button row
    g.setColour (t.textDim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (steps[(std::size_t) current].body, inner, juce::Justification::topLeft, bodyLines);
}

} // namespace rollforge
