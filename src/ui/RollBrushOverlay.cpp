#include "ui/RollBrushOverlay.h"

namespace rollforge
{

namespace
{
    const juce::Colour rollColour { 0xff5ad1c8 };
}

RollBrushOverlay::RollBrushOverlay (int lanes, int steps, int labelW)
    : numLanes (juce::jmax (1, lanes)),
      numSteps (juce::jmax (1, steps)),
      labelWidth (labelW)
{
    setInterceptsMouseClicks (false, false);   // pass-through until the brush is on
}

void RollBrushOverlay::setBrushEnabled (bool shouldBeEnabled)
{
    brushEnabled = shouldBeEnabled;
    setInterceptsMouseClicks (shouldBeEnabled, false);
    if (! shouldBeEnabled)
        dragging = false;
    repaint();
}

void RollBrushOverlay::setRolls (const std::vector<RollRect>& rollsToDraw)
{
    rolls = rollsToDraw;
    repaint();
}

int RollBrushOverlay::laneAt (int y) const noexcept
{
    const int rowH = juce::jmax (1, getHeight() / numLanes);
    return juce::jlimit (0, numLanes - 1, y / rowH);
}

int RollBrushOverlay::stepAt (int x) const noexcept
{
    const int gridW = juce::jmax (1, getWidth() - labelWidth);
    const int cellW = juce::jmax (1, gridW / numSteps);
    return juce::jlimit (0, numSteps - 1, (x - labelWidth) / cellW);
}

juce::Rectangle<int> RollBrushOverlay::cellRect (int lane, int startStep, int len) const noexcept
{
    const int rowH  = juce::jmax (1, getHeight() / numLanes);
    const int gridW = juce::jmax (1, getWidth() - labelWidth);
    const int cellW = juce::jmax (1, gridW / numSteps);
    return { labelWidth + startStep * cellW, lane * rowH, len * cellW, rowH };
}

void RollBrushOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! brushEnabled || e.x < labelWidth)
        return;

    dragging      = true;
    dragLane      = laneAt (e.y);
    dragStartStep = dragCurStep = stepAt (e.x);
    dragStartY    = e.y;
    dragDensity   = 0.5f;
    repaint();
}

void RollBrushOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    dragCurStep = stepAt (e.x);
    const float dy = (float) (dragStartY - e.y);          // up = positive
    dragDensity = juce::jlimit (0.0f, 1.0f, 0.5f + dy / 200.0f);
    repaint();
}

void RollBrushOverlay::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    const int lo     = juce::jmin (dragStartStep, dragCurStep);
    const int hi     = juce::jmax (dragStartStep, dragCurStep);
    const int length = hi - lo + 1;

    if (onRollPainted != nullptr)
        onRollPainted (dragLane, lo, length, dragDensity);

    repaint();
}

void RollBrushOverlay::paint (juce::Graphics& g)
{
    // Existing rolls.
    g.setColour (rollColour.withAlpha (0.26f));
    for (const auto& r : rolls)
        g.fillRect (cellRect (r.lane, r.startStep, r.lengthSteps).reduced (1));

    // In-progress brush stroke.
    if (dragging)
    {
        const int lo  = juce::jmin (dragStartStep, dragCurStep);
        const int hi  = juce::jmax (dragStartStep, dragCurStep);
        auto rr = cellRect (dragLane, lo, hi - lo + 1).reduced (1);

        g.setColour (rollColour.withAlpha (0.45f));
        g.fillRect (rr);
        g.setColour (rollColour);
        g.drawRect (rr, 2);

        // Density indicator: a fill up the right edge (taller = denser).
        auto ind = rr.removeFromRight (5);
        g.fillRect (ind.withTrimmedTop ((int) ((1.0f - dragDensity) * (float) ind.getHeight())));
    }

    if (brushEnabled)
    {
        g.setColour (rollColour.withAlpha (0.9f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("ROLL BRUSH", getLocalBounds().removeFromTop (15).reduced (6, 0),
                    juce::Justification::centredRight, false);
    }
}

} // namespace rollforge
