#include "ui/BrandMark.h"

#include "ui/Theme.h"

namespace rollforge
{

BrandMark::BrandMark()
{
    setInterceptsMouseClicks (false, false);
}

juce::Path BrandMark::anvilGlyph()
{
    // A three-stroke anvil in a unit square, plus two sparks rising off the horn.
    juce::Path p;

    // Body: the classic anvil silhouette — wide face, waisted stem, splayed base.
    p.startNewSubPath (0.06f, 0.44f);
    p.lineTo (0.78f, 0.44f);
    p.lineTo (0.94f, 0.52f);   // the horn
    p.lineTo (0.78f, 0.56f);
    p.lineTo (0.60f, 0.56f);
    p.lineTo (0.54f, 0.74f);
    p.lineTo (0.74f, 0.86f);
    p.lineTo (0.74f, 0.94f);
    p.lineTo (0.16f, 0.94f);
    p.lineTo (0.16f, 0.86f);
    p.lineTo (0.36f, 0.74f);
    p.lineTo (0.30f, 0.56f);
    p.lineTo (0.06f, 0.56f);
    p.closeSubPath();

    return p;
}

void BrandMark::paint (juce::Graphics& g)
{
    const auto& t = theme();
    auto b = getLocalBounds().toFloat();

    const float h = b.getHeight();
    const float glyphSize = h * 0.72f;

    // The mark, struck into the panel: shadow below, metal above.
    {
        auto glyph = anvilGlyph();
        glyph.applyTransform (juce::AffineTransform::scale (glyphSize, glyphSize)
                                  .translated (b.getX(), b.getCentreY() - glyphSize * 0.60f));

        g.setColour (t.panelShadow);
        g.fillPath (glyph, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (t.textDim);
        g.fillPath (glyph);

        // Two sparks off the horn. The only place the mark is allowed colour.
        g.setColour (t.accentHot);
        const float sx = b.getX() + glyphSize * 0.86f;
        const float sy = b.getCentreY() - glyphSize * 0.18f;
        g.fillEllipse (sx, sy - glyphSize * 0.22f, 2.4f, 2.4f);
        g.fillEllipse (sx + glyphSize * 0.14f, sy - glyphSize * 0.40f, 1.8f, 1.8f);
    }

    // The wordmark. One heavy condensed face, two tones, engraved by a 1px shadow.
    {
        auto text = b.withTrimmedLeft (glyphSize + h * 0.22f);

        const float fontHeight = h * 0.62f;
        const auto font = juce::Font (juce::FontOptions (fontHeight, juce::Font::bold)).withHorizontalScale (0.94f);
        g.setFont (font);

        const float rollWidth = juce::GlyphArrangement::getStringWidth (font, "ROLL");

        auto drawWord = [&] (const juce::String& word, float x, juce::Colour colour)
        {
            const auto area = juce::Rectangle<float> (x, text.getY(), text.getWidth(), text.getHeight());
            g.setColour (t.panelShadow);
            g.drawText (word, area.translated (0.0f, 1.0f), juce::Justification::centredLeft, false);
            g.setColour (colour);
            g.drawText (word, area, juce::Justification::centredLeft, false);
        };

        // A faint bloom behind FORGE: the metal is still hot.
        {
            const auto glow = juce::Rectangle<float> (text.getX() + rollWidth - 2.0f, text.getCentreY() - fontHeight * 0.42f,
                                                      fontHeight * 3.2f, fontHeight * 0.84f);
            g.setGradientFill (juce::ColourGradient (t.accentHotDim.withAlpha (0.30f), glow.getCentre(),
                                                     juce::Colours::transparentBlack, glow.getBottomRight(), true));
            g.fillRect (glow);
        }

        drawWord ("ROLL",  text.getX(), t.text);
        drawWord ("FORGE", text.getX() + rollWidth, t.accentHot);
    }
}

} // namespace rollforge
