#include "ui/RollForgeLookAndFeel.h"

namespace rollforge
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    /** A point on a circle, in JUCE's rotary convention: angle 0 is 12 o'clock, clockwise. */
    juce::Point<float> onCircle (juce::Point<float> centre, float radius, float angle) noexcept
    {
        return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
    }

    juce::Path arcPath (juce::Point<float> centre, float radius, float from, float to)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, to, true);
        return p;
    }
}

//==============================================================================
RollForgeLookAndFeel::RollForgeLookAndFeel()
{
    const auto& t = theme();

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::DocumentWindow::textColourId,        t.text);

    setColour (juce::Label::textColourId,                 t.text);
    setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId,          t.buttonFace);
    setColour (juce::TextButton::buttonOnColourId,        t.accentHot);
    setColour (juce::TextButton::textColourOffId,         t.text);
    setColour (juce::TextButton::textColourOnId,          t.background);

    setColour (juce::Slider::textBoxTextColourId,         t.textDim);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,    t.accentCool.withAlpha (0.35f));
    setColour (juce::Slider::rotarySliderFillColourId,    t.ember);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.knobArc);
    setColour (juce::Slider::thumbColourId,               t.knobBodyLight);
    setColour (juce::Slider::trackColourId,               t.sliderFill);
    setColour (juce::Slider::backgroundColourId,          t.sliderTrack);

    setColour (juce::ComboBox::backgroundColourId,        t.buttonFace);
    setColour (juce::ComboBox::textColourId,              t.text);
    setColour (juce::ComboBox::outlineColourId,           t.hairline);
    setColour (juce::ComboBox::arrowColourId,             t.textDim);
    setColour (juce::ComboBox::buttonColourId,            t.buttonFace);

    setColour (juce::PopupMenu::backgroundColourId,             t.panelRaised);
    setColour (juce::PopupMenu::textColourId,                   t.text);
    setColour (juce::PopupMenu::headerTextColourId,             t.textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId,  t.accentCool.withAlpha (0.28f));
    setColour (juce::PopupMenu::highlightedTextColourId,        t.text);

    setColour (juce::TextEditor::backgroundColourId,      t.panel);
    setColour (juce::TextEditor::textColourId,            t.text);
    setColour (juce::TextEditor::outlineColourId,         t.hairline);
    setColour (juce::TextEditor::focusedOutlineColourId,  t.accentCool);
    setColour (juce::TextEditor::highlightColourId,       t.accentCool.withAlpha (0.35f));

    setColour (juce::ScrollBar::thumbColourId,            t.hairline.brighter (0.4f));
    setColour (juce::ScrollBar::trackColourId,            t.panel);

    setColour (juce::TooltipWindow::backgroundColourId,   t.panelRaised);
    setColour (juce::TooltipWindow::textColourId,         t.text);
    setColour (juce::TooltipWindow::outlineColourId,      t.hairline);

    setColour (juce::ListBox::backgroundColourId,         t.panel);
    setColour (juce::ListBox::textColourId,               t.text);

    setColour (juce::ToggleButton::textColourId,          t.text);
    setColour (juce::ToggleButton::tickColourId,          t.accentCool);
    setColour (juce::ToggleButton::tickDisabledColourId,  t.textFaint);
}

void RollForgeLookAndFeel::styleRotary (juce::Slider& slider, bool bipolar)
{
    // 270 degrees, stopped at the bottom — where a real knob's skirt would foul the panel.
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    slider.getProperties().set ("bipolar", bipolar);
}

//==============================================================================
void RollForgeLookAndFeel::drawRaisedPanel (juce::Graphics& g, juce::Rectangle<float> b, float corner)
{
    const auto& t = theme();

    g.setColour (t.panelShadow.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.0f), corner);

    g.setColour (t.panelRaised);
    g.fillRoundedRectangle (b, corner);

    // One light, from above: a lit top edge and a shadowed bottom one.
    g.setColour (t.panelHighlight.withAlpha (0.75f));
    g.drawLine (b.getX() + corner, b.getY() + 0.5f, b.getRight() - corner, b.getY() + 0.5f, 1.0f);
    g.setColour (t.panelShadow);
    g.drawLine (b.getX() + corner, b.getBottom() - 0.5f, b.getRight() - corner, b.getBottom() - 0.5f, 1.0f);

    g.setColour (t.hairline);
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
}

void RollForgeLookAndFeel::drawRecessedWell (juce::Graphics& g, juce::Rectangle<float> b, float corner)
{
    const auto& t = theme();

    g.setColour (t.panel);
    g.fillRoundedRectangle (b, corner);

    // Inverted light: the top edge sits in shadow, the bottom catches it.
    g.setColour (t.panelShadow);
    g.drawLine (b.getX() + corner, b.getY() + 0.5f, b.getRight() - corner, b.getY() + 0.5f, 1.0f);
    g.setColour (t.panelHighlight.withAlpha (0.35f));
    g.drawLine (b.getX() + corner, b.getBottom() - 0.5f, b.getRight() - corner, b.getBottom() - 0.5f, 1.0f);

    g.setColour (t.hairline.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);
}

//==============================================================================
void RollForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float pos, float startAngle, float endAngle,
                                             juce::Slider& slider)
{
    const auto& t = theme();

    const auto  bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    if (radius < 6.0f)
        return;

    const auto centre = bounds.getCentre();
    const float toAngle = startAngle + pos * (endAngle - startAngle);
    const bool  enabled = slider.isEnabled();

    // The value arc rides OUTSIDE the metal. The spec put it at 0.90R over a knurl at
    // 0.86-0.95R, and the arc simply covered the knurl up; a real knob's indicator ring is
    // printed on the faceplate around the knob, not on the knob.
    const float skirtR = radius * 0.84f;   // outer edge of the metal
    const float capR   = radius * 0.68f;

    // 1. Contact shadow. The knob is a physical object sitting ON the faceplate.
    g.setColour (t.panelShadow.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (skirtR * 2.05f, skirtR * 2.05f)
                       .withCentre (centre.translated (0.0f, radius * 0.05f)));

    // 2. The skirt: a fixed machined ring, top-lit.
    {
        const auto skirt = juce::Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (t.knobBodyLight, centre.x, skirt.getY(),
                                                 t.knobSkirt,     centre.x, skirt.getBottom(), false));
        g.fillEllipse (skirt);

        // 3. Its bevel: pale above, dark below.
        g.setColour (t.panelHighlight.withAlpha (0.8f));
        g.strokePath (arcPath (centre, skirtR - 0.5f, -pi * 0.75f, pi * 0.75f), juce::PathStrokeType (1.0f));
        g.setColour (t.panelShadow);
        g.strokePath (arcPath (centre, skirtR - 0.5f, pi * 0.75f, pi * 1.25f), juce::PathStrokeType (1.0f));
    }

    // 4. Knurl: the milled grip that says "aluminium" without a photo texture. Only worth
    //    drawing when a serration is at least a pixel wide, or it turns into grey mush.
    if (skirtR > 14.0f)
    {
        const int serrations = juce::jlimit (24, 72, (int) (skirtR * 1.6f));
        for (int i = 0; i < serrations; ++i)
        {
            const float a = (float) i / (float) serrations * juce::MathConstants<float>::twoPi;
            g.setColour ((i % 2 == 0) ? t.knobBodyLight.withAlpha (0.6f) : t.knobSkirt);
            g.drawLine (juce::Line<float> (onCircle (centre, capR + 1.0f, a),
                                           onCircle (centre, skirtR - 1.5f, a)), 1.0f);
        }
    }

    // 5-6. The cap, domed by a single vertical gradient and a shaded lower lip.
    {
        const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (t.knobBody.brighter (0.28f), centre.x, cap.getY(),
                                                 t.knobBody.darker (0.40f),   centre.x, cap.getBottom(), false));
        g.fillEllipse (cap);

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.strokePath (arcPath (centre, capR - 0.8f, pi * 0.6f, pi * 1.4f), juce::PathStrokeType (1.6f));
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.strokePath (arcPath (centre, capR - 0.8f, -pi * 0.55f, pi * 0.55f), juce::PathStrokeType (1.0f));

        // 7. One specular glint, up and to the left. Only ever one.
        const auto glint = juce::Rectangle<float> (capR * 1.10f, capR * 0.85f)
                               .withCentre (centre.translated (-capR * 0.26f, -capR * 0.30f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (enabled ? 0.17f : 0.05f),
                                                 glint.getCentre(),
                                                 juce::Colours::transparentWhite, glint.getBottomRight(), true));
        g.fillEllipse (glint);
    }

    // 8-9. The value arc: unfilled track, then the live value over it with a soft bloom.
    {
        const float arcR   = radius * 0.94f;
        const float stroke = juce::jmax (2.0f, radius * 0.085f);

        g.setColour (t.knobArc);
        g.strokePath (arcPath (centre, arcR, startAngle, endAngle),
                      juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (enabled)
        {
            auto arcColour = t.ember;
            if (const auto v = slider.getProperties()["arcColour"]; ! v.isVoid())
                arcColour = juce::Colour ((juce::uint32) (int) v);

            // A bipolar control (an EQ band, a tone tilt) grows from the centre detent,
            // because that is where "no change" is. Growing from the minimum would say
            // that -12 dB is the resting state.
            const bool  bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
            const float from    = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

            if (std::abs (toAngle - from) > 0.001f)
            {
                const auto path = arcPath (centre, arcR, juce::jmin (from, toAngle), juce::jmax (from, toAngle));
                g.setColour (arcColour.withAlpha (0.22f));
                g.strokePath (path, juce::PathStrokeType (stroke * 1.9f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
                g.setColour (arcColour);
                g.strokePath (path, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
        }
    }

    // 10. The pointer, engraved: a dark groove with the light catching one side of it.
    {
        const auto tip  = onCircle (centre, capR - 2.0f, toAngle);
        const auto root = onCircle (centre, capR * 0.30f, toAngle);
        const float w   = juce::jmax (1.6f, radius * 0.075f);

        g.setColour (t.panelShadow);
        g.drawLine (juce::Line<float> (root, tip), w + 1.4f);
        g.setColour (enabled ? t.knobPointer : t.textFaint);
        g.drawLine (juce::Line<float> (root, tip), w);
    }
}

//==============================================================================
void RollForgeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto& t = theme();

    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const bool horizontal = style == juce::Slider::LinearHorizontal;
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();

    auto fill = t.sliderFill;
    if (const auto v = slider.getProperties()["arcColour"]; ! v.isVoid())
        fill = juce::Colour ((juce::uint32) (int) v);
    if (! slider.isEnabled())
        fill = t.textFaint;

    // The groove: recessed, so the light lands on its LOWER lip.
    const float thickness = 6.0f;
    const auto groove = horizontal
        ? juce::Rectangle<float> (area.getX(), area.getCentreY() - thickness * 0.5f, area.getWidth(), thickness)
        : juce::Rectangle<float> (area.getCentreX() - thickness * 0.5f, area.getY(), thickness, area.getHeight());

    g.setColour (t.sliderTrack);
    g.fillRoundedRectangle (groove, thickness * 0.5f);
    g.setColour (t.panelShadow);
    g.drawLine (groove.getX(), groove.getY() + 0.5f, groove.getRight(), groove.getY() + 0.5f, 1.0f);
    g.setColour (t.panelHighlight.withAlpha (0.35f));
    g.drawLine (groove.getX(), groove.getBottom() - 0.5f, groove.getRight(), groove.getBottom() - 0.5f, 1.0f);

    // The travelled part.
    if (horizontal)
    {
        auto filled = groove.withRight (juce::jlimit (groove.getX(), groove.getRight(), sliderPos));
        if (filled.getWidth() > 1.0f)
        {
            g.setColour (fill.withAlpha (0.85f));
            g.fillRoundedRectangle (filled, thickness * 0.5f);
        }
    }
    else
    {
        auto filled = groove.withTop (juce::jlimit (groove.getY(), groove.getBottom(), sliderPos));
        if (filled.getHeight() > 1.0f)
        {
            g.setColour (fill.withAlpha (0.85f));
            g.fillRoundedRectangle (filled, thickness * 0.5f);
        }
    }

    // The cap: a milled thumb with a grip score down the middle.
    const float capW = horizontal ? 13.0f : area.getWidth() - 2.0f;
    const float capH = horizontal ? juce::jmin (22.0f, area.getHeight()) : 13.0f;
    const auto cap = juce::Rectangle<float> (capW, capH).withCentre (
        horizontal ? juce::Point<float> (sliderPos, area.getCentreY())
                   : juce::Point<float> (area.getCentreX(), sliderPos));

    g.setColour (t.panelShadow.withAlpha (0.6f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (t.knobBodyLight, cap.getCentreX(), cap.getY(),
                                             t.knobBody,      cap.getCentreX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (t.panelHighlight.withAlpha (0.9f));
    g.drawLine (cap.getX() + 2.0f, cap.getY() + 0.5f, cap.getRight() - 2.0f, cap.getY() + 0.5f, 1.0f);
    g.setColour (t.knobSkirt);
    if (horizontal)
        g.drawLine (cap.getCentreX(), cap.getY() + 4.0f, cap.getCentreX(), cap.getBottom() - 4.0f, 1.0f);
    else
        g.drawLine (cap.getX() + 4.0f, cap.getCentreY(), cap.getRight() - 4.0f, cap.getCentreY(), 1.0f);
}

//==============================================================================
void RollForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool isHighlighted, bool isDown)
{
    const auto& t = theme();
    const auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    constexpr float corner = 5.0f;

    const bool lit = button.getToggleState();
    auto face = backgroundColour;
    if (! lit && isHighlighted)
        face = t.buttonFaceHover;

    if (lit)
    {
        // An engaged control glows: the colour spills past its own edge.
        g.setColour (face.withAlpha (0.30f));
        g.fillRoundedRectangle (b.expanded (2.5f), corner + 2.0f);
    }
    else
    {
        g.setColour (t.panelShadow.withAlpha (0.5f));
        g.fillRoundedRectangle (b.translated (0.0f, 1.0f), corner);
    }

    if (isDown)
    {
        // Pressed: the light stops reaching it.
        g.setColour (face.darker (0.35f));
        g.fillRoundedRectangle (b, corner);
        g.setColour (t.panelShadow);
        g.drawLine (b.getX() + corner, b.getY() + 0.5f, b.getRight() - corner, b.getY() + 0.5f, 1.0f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (face.brighter (0.10f), b.getCentreX(), b.getY(),
                                                 face.darker (0.12f),   b.getCentreX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, corner);
        g.setColour ((lit ? juce::Colours::white : t.panelHighlight).withAlpha (lit ? 0.35f : 0.7f));
        g.drawLine (b.getX() + corner, b.getY() + 0.7f, b.getRight() - corner, b.getY() + 0.7f, 1.0f);
        g.setColour (t.panelShadow.withAlpha (0.8f));
        g.drawLine (b.getX() + corner, b.getBottom() - 0.7f, b.getRight() - corner, b.getBottom() - 0.7f, 1.0f);
    }

    g.setColour (lit ? face.brighter (0.3f) : t.hairline);
    g.drawRoundedRectangle (b, corner, 1.0f);
}

void RollForgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool isDown)
{
    const auto& t = theme();
    const bool lit = button.getToggleState();

    auto colour = lit ? button.findColour (juce::TextButton::textColourOnId)
                      : button.findColour (juce::TextButton::textColourOffId);
    if (! button.isEnabled())
        colour = t.textFaint;

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (colour);
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().reduced (6, 0).translated (0, isDown ? 1 : 0),
                      juce::Justification::centred, 1, 0.9f);
}

//==============================================================================
void RollForgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    const auto& t = theme();
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    constexpr float corner = 4.0f;

    g.setColour (t.panelShadow.withAlpha (0.45f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.0f), corner);

    g.setGradientFill (juce::ColourGradient (t.buttonFace.brighter (0.08f), b.getCentreX(), b.getY(),
                                             t.buttonFace.darker (0.10f),   b.getCentreX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, corner);

    g.setColour (t.panelHighlight.withAlpha (0.6f));
    g.drawLine (b.getX() + corner, b.getY() + 0.7f, b.getRight() - corner, b.getY() + 0.7f, 1.0f);
    g.setColour (box.hasKeyboardFocus (false) ? t.accentCool.withAlpha (0.7f) : t.hairline);
    g.drawRoundedRectangle (b, corner, 1.0f);

    // A drawn chevron, not a glyph: it scales with the UI and never picks up a font.
    const float cx = b.getRight() - 14.0f;
    const float cy = b.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx,        cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (t.textDim);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void RollForgeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool isHighlighted, bool)
{
    const auto& t = theme();
    const auto box = juce::Rectangle<float> (14.0f, 14.0f)
                         .withCentre ({ 9.0f, (float) button.getHeight() * 0.5f });

    g.setColour (isHighlighted ? t.buttonFaceHover : t.buttonFace);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (t.hairline);
    g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY());
        tick.lineTo (box.getCentreX() - 0.5f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
        g.setColour (button.isEnabled() ? t.accentCool : t.textFaint);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (button.isEnabled() ? t.text : t.textFaint);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (button.getButtonText(),
                button.getLocalBounds().withTrimmedLeft (24),
                juce::Justification::centredLeft, true);
}

void RollForgeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto& t = theme();
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    drawRaisedPanel (g, b.reduced (1.0f), 5.0f);
}

//==============================================================================
juce::Font RollForgeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.6f)));
}

juce::Font RollForgeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

juce::Font RollForgeLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

} // namespace rollforge
