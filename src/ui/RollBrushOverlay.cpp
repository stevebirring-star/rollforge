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
    updatePreview();
    repaint();
}

void RollBrushOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    dragCurStep = stepAt (e.x);
    const float dy = (float) (dragStartY - e.y);          // up = positive
    dragDensity = juce::jlimit (0.0f, 1.0f, 0.5f + dy / 200.0f);
    updatePreview();
    repaint();
}

void RollBrushOverlay::updatePreview() noexcept
{
    if (getHitCount == nullptr)
    {
        previewHits = 0;
        return;
    }
    const int lo  = juce::jmin (dragStartStep, dragCurStep);
    const int len = juce::jmax (dragStartStep, dragCurStep) - lo + 1;
    previewHits = getHitCount (dragLane, lo, len, dragDensity);
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
        const int len = hi - lo + 1;
        auto rr = cellRect (dragLane, lo, len).reduced (1);
        const auto strokeRect = rr;               // capture before the edge meter trims rr

        g.setColour (rollColour.withAlpha (0.45f));
        g.fillRect (rr);
        g.setColour (rollColour);
        g.drawRect (rr, 2);

        // Density meter: a fill up the right edge (taller = denser).
        auto ind = rr.removeFromRight (5);
        g.fillRect (ind.withTrimmedTop ((int) ((1.0f - dragDensity) * (float) ind.getHeight())));

        // Live readout: density %, span (steps + beats), and the hit count this
        // stroke will compile to. Anchored above the stroke (below if near the top).
        const int boxW = 154;
        const int boxH = (getHitCount != nullptr) ? 52 : 38;
        int bx = strokeRect.getX();
        int by = strokeRect.getY() - boxH - 4;
        if (by < 0)
            by = strokeRect.getBottom() + 4;
        bx = juce::jlimit (0, juce::jmax (0, getWidth()  - boxW), bx);
        by = juce::jlimit (0, juce::jmax (0, getHeight() - boxH), by);
        const juce::Rectangle<int> box (bx, by, boxW, boxH);

        g.setColour (juce::Colour (0xff141418).withAlpha (0.94f));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (rollColour.withAlpha (0.9f));
        g.drawRoundedRectangle (box.toFloat(), 4.0f, 1.0f);

        auto txt = box.reduced (8, 5);
        const int lineH = 14;
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        g.setColour (juce::Colour (0xffe8e8ec));
        g.drawText ("Density  " + juce::String (juce::roundToInt (dragDensity * 100.0f)) + "%",
                    txt.removeFromTop (lineH), juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (0xffb8b8c0));
        g.drawText ("Span  " + juce::String (len) + (len == 1 ? " step" : " steps")
                        + "  ·  " + juce::String (len / 4.0, 2) + " beats",
                    txt.removeFromTop (lineH), juce::Justification::centredLeft, false);

        if (getHitCount != nullptr)
        {
            g.setColour (rollColour);
            g.drawText ("Hits  ~" + juce::String (previewHits),
                        txt.removeFromTop (lineH), juce::Justification::centredLeft, false);
        }
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
