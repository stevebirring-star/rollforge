#include "ui/SimilarityMap.h"

#include "ui/RollForgeLookAndFeel.h"
#include "ui/Theme.h"

#include <cmath>

namespace rollforge
{

namespace
{
    constexpr float dotRadius      = 4.5f;
    constexpr float hitRadius      = 11.0f;   // generous: a 4 px dot is hard to hit exactly
    constexpr float axisGutter     = 18.0f;   // room for the axis captions
    constexpr float plotInset      = 14.0f;   // keeps a dot on the edge from being clipped
}

SimilarityMap::SimilarityMap()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SimilarityMap::setCorpus (const std::vector<LibraryEntry>& entries,
                               const Similarity::Projection& proj)
{
    corpus     = entries;
    projection = proj;
    selected   = -1;
    hovered    = -1;
    repaint();
}

void SimilarityMap::setCategoryFilter (std::optional<SoundCategory> category)
{
    filter = category;
    repaint();
}

void SimilarityMap::setSelected (int index)
{
    if (selected == index)
        return;
    selected = index;
    repaint();
}

bool SimilarityMap::isDimmed (int index) const
{
    return filter.has_value() && corpus[(std::size_t) index].category != *filter;
}

juce::Rectangle<float> SimilarityMap::plotArea() const
{
    return getLocalBounds().toFloat()
               .withTrimmedBottom (axisGutter)
               .withTrimmedTop (axisGutter)
               .reduced (plotInset);
}

juce::Point<float> SimilarityMap::positionOf (int index) const
{
    const auto  area = plotArea();
    const auto& p    = projection.points[(std::size_t) index];

    // y is flipped: PCA's second axis grows upward, and so should the picture.
    return { area.getX() + p.x * area.getWidth(),
             area.getBottom() - p.y * area.getHeight() };
}

int SimilarityMap::indexAt (juce::Point<float> position) const
{
    int   best     = -1;
    float bestDist = hitRadius * hitRadius;

    // Later points win ties, matching the paint order — you can always click what you see.
    for (int i = 0; i < (int) corpus.size(); ++i)
    {
        if (isDimmed (i))
            continue;

        const float d = position.getDistanceSquaredFrom (positionOf (i));
        if (d <= bestDist)
        {
            bestDist = d;
            best     = i;
        }
    }
    return best;
}

void SimilarityMap::resized() { repaint(); }

void SimilarityMap::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
}

void SimilarityMap::mouseExit (const juce::MouseEvent&)
{
    if (hovered >= 0)
    {
        hovered = -1;
        repaint();
    }
}

void SimilarityMap::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i < 0)
        return;

    setSelected (i);

    if (e.mods.isPopupMenu())
    {
        if (onContextMenu != nullptr)
            onContextMenu (i);
    }
    else if (onAudition != nullptr)
    {
        onAudition (i);
    }
}

void SimilarityMap::paint (juce::Graphics& g)
{
    const auto& t    = theme();
    const auto  area = plotArea();

    RollForgeLookAndFeel::drawRecessedWell (g, getLocalBounds().toFloat(), 6.0f);

    if (corpus.empty() || projection.points.size() != corpus.size())
    {
        g.setColour (t.textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Scan a folder to see your library laid out by sound",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Graph paper, not a crosshair. Each axis is rescaled to its own min..max, so there is
    // no origin to mark — a centre cross would invent one and invite people to read
    // "positive" and "negative" into a picture that has neither.
    g.setColour (t.hairline.withAlpha (0.4f));
    for (int i = 1; i < 4; ++i)
    {
        const float f = (float) i / 4.0f;
        const float x = area.getX() + f * area.getWidth();
        const float y = area.getY() + f * area.getHeight();
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
        g.drawLine (area.getX(), y, area.getRight(), y, 1.0f);
    }

    // Axis captions: whichever feature loads most heavily on each axis. They are the honest
    // answer to "what does left-to-right mean here?", and they change with the library.
    // Arrowheads are drawn, not typed — a UTF-8 glyph in a source literal came out as
    // mojibake, and a font that lacks it would have come out as a box.
    const auto caption = [&] (juce::Rectangle<int> strip, const char* name, bool horizontal)
    {
        g.setColour (t.textFaint);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

        const int  textWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (
                                   juce::Font (juce::FontOptions (10.0f, juce::Font::bold)), name));

        // The horizontal caption belongs under the middle of the axis it names; the vertical
        // one sits at the top-left, where a centred label would read as a title instead.
        const auto text = horizontal ? strip.withSizeKeepingCentre (textWidth, strip.getHeight())
                                     : strip.withWidth (textWidth);
        g.drawText (name, text, juce::Justification::centred);

        const float cy = (float) strip.getCentreY();
        const float a  = 3.5f;   // arrowhead half-height
        juce::Path arrows;

        if (horizontal)
        {
            const float left  = (float) text.getX() - 8.0f;
            const float right = (float) text.getRight() + 8.0f;
            arrows.addTriangle (left, cy, left + 6.0f, cy - a, left + 6.0f, cy + a);
            arrows.addTriangle (right, cy, right - 6.0f, cy - a, right - 6.0f, cy + a);
        }
        else
        {
            // Points up: the vertical axis grows the way the picture does.
            const float x = (float) text.getX() - 10.0f;
            arrows.addTriangle (x, cy - 4.0f, x - a, cy + 2.0f, x + a, cy + 2.0f);
        }

        g.fillPath (arrows);
    };

    caption (getLocalBounds().removeFromBottom ((int) axisGutter),
             Similarity::featureName (projection.dominantFeature (false)), true);
    caption (getLocalBounds().removeFromTop ((int) axisGutter).withTrimmedLeft (20),
             Similarity::featureName (projection.dominantFeature (true)), false);

    // Dimmed points first, so a filtered-in dot is never buried under context.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < (int) corpus.size(); ++i)
        {
            const bool dim = isDimmed (i);
            if (dim != (pass == 0))
                continue;

            const auto  p      = positionOf (i);
            const auto  colour = t.colourFor (corpus[(std::size_t) i].category);
            const bool  isHot  = (i == hovered || i == selected);
            const float r      = dotRadius * (isHot ? 1.5f : 1.0f);

            if (dim)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
                continue;
            }

            if (isHot)
            {
                g.setColour (colour.withAlpha (0.25f));
                g.fillEllipse (p.x - r * 2.2f, p.y - r * 2.2f, r * 4.4f, r * 4.4f);
            }

            g.setColour (colour.withAlpha (0.9f));
            g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);

            if (i == selected)
            {
                g.setColour (t.text);
                g.drawEllipse (p.x - r - 3.0f, p.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, 1.4f);
            }
        }
    }

    // The hovered sample's name, in a chip that stays inside the view even at the edges.
    if (hovered >= 0 && hovered < (int) corpus.size())
    {
        const auto& e    = corpus[(std::size_t) hovered];
        const auto  p    = positionOf (hovered);
        const auto  font = juce::FontOptions (12.0f);

        g.setFont (font);
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (font), e.name)) + 16;
        const int h = 20;

        auto chip = juce::Rectangle<int> ((int) p.x - w / 2, (int) p.y - h - 12, w, h)
                        .constrainedWithin (getLocalBounds());

        g.setColour (t.panelRaised);
        g.fillRoundedRectangle (chip.toFloat(), 4.0f);
        g.setColour (t.hairline);
        g.drawRoundedRectangle (chip.toFloat(), 4.0f, 1.0f);
        g.setColour (t.text);
        g.drawText (e.name, chip, juce::Justification::centred);
    }
}

} // namespace rollforge
