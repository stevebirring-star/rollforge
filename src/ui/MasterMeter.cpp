#include "ui/MasterMeter.h"

#include "ui/RollForgeLookAndFeel.h"

namespace rollforge
{

namespace
{
    constexpr float degToRad = juce::MathConstants<float>::pi / 180.0f;

    /** The printed scale. Long ticks carry a numeral; short ones don't. Fewer numerals
        than a full-size meter, because at 100 px they collide into mush. */
    struct Tick { float vuDb; bool major; const char* label; };
    const Tick ticks[] = {
        { -20.0f, true,  "20" }, { -14.0f, false, nullptr }, { -10.0f, true,  "10" },
        { -7.0f,  false, nullptr }, { -5.0f, true,  "5"  },  { -3.0f,  false, nullptr },
        { -1.0f,  false, nullptr },
        { 0.0f,   true,  "0"  },  { 1.0f,  false, nullptr }, { 2.0f,  false, nullptr },
        { 3.0f,   true,  "+3" },
    };

    /** Where a needle at deflection f points, in radians clockwise from straight up. */
    float angleFor (float deflection) noexcept
    {
        return (-45.0f + juce::jlimit (0.0f, 1.0f, deflection) * 90.0f) * degToRad;
    }

    juce::Point<float> onArc (juce::Point<float> pivot, float radius, float angle) noexcept
    {
        return { pivot.x + radius * std::sin (angle), pivot.y - radius * std::cos (angle) };
    }
}

//==============================================================================
MasterMeter::MasterMeter (const OutputMeter& source) : meter (source)
{
    setTooltip ("Master output level, left and right channels, measured after the limiter — "
                "what actually reaches your speakers.\n"
                "The needles are true VU: 300 ms averaging, so they read loudness and ignore "
                "single transients. The slim bar on each face is sample peak, and CLIP lights "
                "if the output goes over.");
    startTimerHz (30);
}

void MasterMeter::timerCallback()
{
    bool dirty = false;

    for (int ch = 0; ch < 2; ++ch)
    {
        // The engine already applied VU ballistics; this only bridges the 30 Hz gap so a
        // needle never teleports between frames.
        const float target = meter.getVuDeflection (ch);
        const float next   = displayed[ch] + (target - displayed[ch]) * 0.55f;
        if (std::abs (next - displayed[ch]) > 0.0005f) { displayed[ch] = next; dirty = true; }

        const float peak = juce::jlimit (0.0f, 1.0f, (meter.getPeakDbfs (ch) + 40.0f) / 40.0f);
        if (std::abs (peak - peakBar[ch]) > 0.002f) { peakBar[ch] = peak; dirty = true; }
    }

    // The clip lamp latches, or a single over-sample would flash for one frame and be
    // gone before anyone saw it — which is precisely when you need to know.
    const bool over = meter.getPeakDbfs (0) >= clipDbfs || meter.getPeakDbfs (1) >= clipDbfs;
    if (over)
    {
        clipHold = clipHoldTicks;
        dirty = true;
    }
    else if (clipHold > 0)
    {
        --clipHold;
        dirty = true;
    }

    if (dirty)
        repaint();
}

//==============================================================================
void MasterMeter::paint (juce::Graphics& g)
{
    const auto& t = theme();
    auto b = getLocalBounds().toFloat();

    // One bezel, two faces: L and R read as a single instrument, the way a rack meter does.
    RollForgeLookAndFeel::drawRaisedPanel (g, b, 5.0f);
    auto inner = b.reduced (4.0f);

    const float lampWidth = 24.0f;
    auto lampCol = inner.removeFromRight (lampWidth);

    const float gap = 4.0f;
    const float faceW = (inner.getWidth() - gap) * 0.5f;
    drawFace (g, inner.removeFromLeft (faceW), 0, "L");
    inner.removeFromLeft (gap);
    drawFace (g, inner, 1, "R");

    // The clip lamp: dark until it matters, then unmistakable.
    {
        const bool lit = clipHold > 0;
        const auto lamp = juce::Rectangle<float> (9.0f, 9.0f)
                              .withCentre ({ lampCol.getCentreX(), lampCol.getY() + 12.0f });

        if (lit)
        {
            g.setColour (t.meterPeakLamp.withAlpha (0.35f));
            g.fillEllipse (lamp.expanded (4.0f));
        }
        g.setColour (lit ? t.meterPeakLamp : t.meterPeakLamp.withAlpha (0.16f));
        g.fillEllipse (lamp);
        g.setColour (t.panelShadow);
        g.drawEllipse (lamp, 1.0f);

        g.setColour (lit ? t.meterPeakLamp : t.textFaint);
        g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
        g.drawText ("CLIP", lampCol.withTop (lamp.getBottom() + 3.0f).withHeight (10.0f),
                    juce::Justification::centred, false);
    }
}

void MasterMeter::drawFace (juce::Graphics& g, juce::Rectangle<float> face, int channel,
                            const juce::String& label)
{
    const auto& t = theme();
    constexpr float corner = 3.0f;
    const auto bezel = face;

    // The cavity the glass sits in.
    g.setColour (t.panelShadow);
    g.fillRoundedRectangle (face.expanded (1.0f), corner + 1.0f);

    // Backlit glass: brightest behind the middle of the arc, falling to a vignette.
    {
        juce::ColourGradient glow (t.meterGlass.brighter (0.35f),
                                   face.getCentreX(), face.getY() + face.getHeight() * 0.55f,
                                   t.meterGlassEdge, face.getX(), face.getBottom(), true);
        glow.addColour (0.55, t.meterGlass);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (face, corner);
    }

    // Peak bar down the right inner edge — the honest digital reading, kept slim and away
    // from the needle so the two instruments are never confused with each other.
    auto peakLane = face.removeFromRight (4.0f).reduced (1.0f, 4.0f);
    {
        g.setColour (t.panelShadow.withAlpha (0.6f));
        g.fillRoundedRectangle (peakLane, 1.0f);

        const float h = peakLane.getHeight() * juce::jlimit (0.0f, 1.0f, peakBar[channel]);
        if (h > 0.5f)
        {
            const float dbfs = meter.getPeakDbfs (channel);
            const auto colour = dbfs >= -6.0f  ? t.meterRedZone
                              : dbfs >= -18.0f ? t.colourFor (SoundCategory::Snare)
                                               : t.colourFor (SoundCategory::Perc);
            g.setColour (colour);
            g.fillRoundedRectangle (peakLane.withTop (peakLane.getBottom() - h), 1.0f);
        }
    }

    // The nameplate lives under the movement; the arc gets what's left.
    auto plate = face.removeFromBottom (11.0f);
    auto glass = face;

    // Geometry. The pivot IS the hub, near the bottom of the window, and the radius is
    // whatever lets the full +-45 degree sweep fit inside the glass. Deriving the radius
    // from the height instead — as the spec's 1.18*H pivot did — throws the +3 mark clean
    // outside the bezel on any face narrower than it is tall.
    const auto pivot = juce::Point<float> (glass.getCentreX(), glass.getBottom() - 3.0f);
    const float halfSweepSin = std::sin (45.0f * degToRad);
    const float rArc = juce::jmin ((glass.getWidth() * 0.5f - 7.0f) / halfSweepSin,
                                   glass.getHeight() - 6.0f);

    // The printed scale, then the red zone laid over it.
    {
        juce::Path scale;
        scale.addCentredArc (pivot.x, pivot.y, rArc, rArc, 0.0f,
                             angleFor (0.0f), angleFor (1.0f), true);
        g.setColour (t.meterScale.withAlpha (0.55f));
        g.strokePath (scale, juce::PathStrokeType (1.0f));

        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, rArc, rArc, 0.0f,
                           angleFor (OutputMeter::deflectionForVuDb (0.0f)), angleFor (1.0f), true);
        g.setColour (t.meterRedZone);
        g.strokePath (red, juce::PathStrokeType (2.0f));

        g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
        for (const auto& tick : ticks)
        {
            const float a = angleFor (OutputMeter::deflectionForVuDb (tick.vuDb));
            const float len = tick.major ? 6.0f : 3.5f;
            const bool  hot = tick.vuDb >= 0.0f;

            g.setColour (hot ? t.meterRedZone : t.meterScale.withAlpha (0.85f));
            g.drawLine (juce::Line<float> (onArc (pivot, rArc, a), onArc (pivot, rArc - len, a)),
                        tick.major ? 1.4f : 1.0f);

            if (tick.label != nullptr && rArc > 40.0f)
            {
                const auto p = onArc (pivot, rArc - 11.0f, a);
                g.setColour (hot ? t.meterRedZone : t.meterScale);
                g.drawText (tick.label, juce::Rectangle<float> (16.0f, 9.0f).withCentre (p),
                            juce::Justification::centred, false);
            }
        }
    }

    // The engraved nameplate, McIntosh-style: the meter carries the brand.
    {
        g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
        g.setColour (t.meterScale.withAlpha (0.40f));
        g.drawText ("ROLLFORGE  VU", plate, juce::Justification::centred, false);

        g.setColour (t.meterScale.withAlpha (0.75f));
        g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
        g.drawText (label, glass.withWidth (13.0f).withTop (glass.getY() + 1.0f).withHeight (10.0f),
                    juce::Justification::centred, false);
    }

    // The needle: a tapered blade pivoting at the hub, with its shadow on the printed face
    // a millimetre below it. It floats under glass; it is not painted on.
    {
        const float a = angleFor (displayed[channel]);
        const auto tip  = onArc (pivot, rArc - 2.0f, a);
        const auto root = onArc (pivot, 2.0f, a);

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawLine (juce::Line<float> (root.translated (1.0f, 1.5f), tip.translated (1.0f, 1.5f)), 1.7f);

        g.setColour (t.meterNeedle);
        g.drawLine (juce::Line<float> (root, tip), 1.5f);

        const auto hub = juce::Rectangle<float> (6.5f, 6.5f).withCentre (pivot);
        g.setGradientFill (juce::ColourGradient (t.knobBodyLight, hub.getTopLeft(),
                                                 t.knobSkirt, hub.getBottomRight(), false));
        g.fillEllipse (hub);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (hub.withSizeKeepingCentre (2.0f, 2.0f).translated (-0.8f, -0.8f));
    }

    // Glass: one faint diagonal sweep, and a vignette. Never a cartoon shine.
    {
        juce::Path gloss;
        gloss.startNewSubPath (glass.getX(), glass.getY() + glass.getHeight() * 0.60f);
        gloss.lineTo (glass.getX(), glass.getY());
        gloss.lineTo (glass.getX() + glass.getWidth() * 0.60f, glass.getY());
        gloss.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (gloss);

        g.setColour (t.panelShadow.withAlpha (0.7f));
        g.drawRoundedRectangle (bezel.reduced (0.5f), corner, 1.0f);
    }
}

} // namespace rollforge
