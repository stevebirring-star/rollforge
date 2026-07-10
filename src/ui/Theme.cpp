#include "ui/Theme.h"

namespace rollforge
{

namespace
{
    juce::Colour c (juce::uint32 argb) noexcept { return juce::Colour (argb); }

    // -------------------------------------------------------------------------
    // FORGE — forged steel, hot metal, blue meter glass.
    //
    // Cold gunmetal so the orange has somewhere to burn, and a McIntosh-blue VU face as
    // the one large area of saturated colour. The name does the work: a forge is dark,
    // and the only bright things in it are the metal and the fire.
    // -------------------------------------------------------------------------
    Theme makeForge()
    {
        Theme t;
        t.name           = "Forge";
        t.background     = c (0xff12141a);
        t.backgroundDeep = c (0xff0b0d11);
        t.panel          = c (0xff191c23);
        t.panelRaised    = c (0xff232730);
        t.panelHighlight = c (0xff363d4a);
        t.panelShadow    = c (0xff090b0f);
        t.hairline       = c (0xff2b303a);

        t.text      = c (0xffe9ecf2);
        t.textDim   = c (0xff9aa1ad);
        t.textFaint = c (0xff59606e);

        t.accentHot     = c (0xffff7a1a);   // forge orange
        t.accentHotDim  = c (0xff6d3410);
        t.ember         = c (0xffffb43a);   // cooled ember: knob arcs
        t.emberDim      = c (0xff5c4014);
        t.accentCool    = c (0xff3f8cff);   // meter blue
        t.accentCoolDim = c (0xff1b3a63);

        t.knobBody      = c (0xff2c313b);
        t.knobBodyLight = c (0xff474e5c);
        t.knobSkirt     = c (0xff14171d);
        t.knobPointer   = c (0xffe9ecf2);
        t.knobArc       = c (0xff2b303a);

        t.sliderTrack     = c (0xff14171d);
        t.sliderFill      = c (0xff3f8cff);
        t.buttonFace      = c (0xff232730);
        t.buttonFaceHover = c (0xff2e3440);

        t.meterGlass     = c (0xff0e3d80);   // backlit blue, McIntosh
        t.meterGlassEdge = c (0xff06183a);
        t.meterScale     = c (0xffd2e5ff);
        t.meterNeedle    = c (0xff07090d);   // black needle on lit glass
        t.meterRedZone   = c (0xffff4d3d);
        t.meterPeakLamp  = c (0xffff3b30);

        t.category = {{
            c (0xffff6b3d),   // Kick      — hot orange-red   (luma .55)
            c (0xffffc63c),   // Snare     — brass gold       (.75) pulled off the green
            c (0xffff5fa8),   // Clap      — magenta          (.58) survives red/green CB
            c (0xff4fd6e8),   // HatClosed — bright cyan      (.75)
            c (0xff1f8fa8),   // HatOpen   — deep teal        (.42) same hue, darker: the
                              //                                    hats separate by LIGHTNESS
            c (0xff9b7bff),   // Tom       — violet           (.55)
            c (0xff57dd86),   // Perc      — spring green     (.72)
            c (0xff93a0b5),   // Fx        — pewter           (.63) desaturated: a cymbal
                              //                                    never competes with a pad
            c (0xff6c7383),   // Unknown   — slate            (.46)
        }};
        return t;
    }

    // -------------------------------------------------------------------------
    // STUDIO — console grey, oxide red, cream VU faces.
    //
    // The Neve/API language: a warm grey desk, one oxide-red engage colour, amber lamps,
    // and the classic ivory VU with a black needle. Quieter and more serious than Forge.
    // -------------------------------------------------------------------------
    Theme makeStudio()
    {
        Theme t;
        t.name           = "Studio";
        t.background     = c (0xff1a1a1c);
        t.backgroundDeep = c (0xff101011);
        t.panel          = c (0xff222325);
        t.panelRaised    = c (0xff2e3033);
        t.panelHighlight = c (0xff44474b);
        t.panelShadow    = c (0xff0e0e10);
        t.hairline       = c (0xff36383c);

        t.text      = c (0xfff0efea);
        t.textDim   = c (0xffa5a49e);
        t.textFaint = c (0xff65645f);

        t.accentHot     = c (0xffd0392b);   // oxide red — engage
        t.accentHotDim  = c (0xff5c1a14);
        t.ember         = c (0xffe0a03a);
        t.emberDim      = c (0xff5a3f16);
        t.accentCool    = c (0xfff2a93b);   // amber lamp — running
        t.accentCoolDim = c (0xff6b4a19);

        t.knobBody      = c (0xff36383c);
        t.knobBodyLight = c (0xff55585e);
        t.knobSkirt     = c (0xff161719);
        t.knobPointer   = c (0xfff0efea);
        t.knobArc       = c (0xff36383c);

        t.sliderTrack     = c (0xff161719);
        t.sliderFill      = c (0xfff2a93b);
        t.buttonFace      = c (0xff2e3033);
        t.buttonFaceHover = c (0xff3a3d41);

        t.meterGlass     = c (0xffe8dfc8);   // ivory face
        t.meterGlassEdge = c (0xffb9b09a);
        t.meterScale     = c (0xff23211c);   // printed black scale
        t.meterNeedle    = c (0xff17150f);
        t.meterRedZone   = c (0xffc0271c);
        t.meterPeakLamp  = c (0xffd0392b);

        t.category = {{
            c (0xffd9553c),   // Kick
            c (0xffe8b93c),   // Snare
            c (0xffd05a8c),   // Clap
            c (0xff5fbcc4),   // HatClosed
            c (0xff2d7f8a),   // HatOpen
            c (0xff8f7ac4),   // Tom
            c (0xff6fbf7a),   // Perc
            c (0xff97968f),   // Fx
            c (0xff6e6d68),   // Unknown
        }};
        return t;
    }

    // -------------------------------------------------------------------------
    // MIDNIGHT — deep indigo, ice cyan, magenta.
    //
    // The modern-plugin look (XO, Arcade, Baby Audio): near-black indigo, high-saturation
    // accents, glass meters. Colder and more electronic than the other two.
    // -------------------------------------------------------------------------
    Theme makeMidnight()
    {
        Theme t;
        t.name           = "Midnight";
        t.background     = c (0xff0d0f1c);
        t.backgroundDeep = c (0xff070812);
        t.panel          = c (0xff141830);
        t.panelRaised    = c (0xff1d2242);
        t.panelHighlight = c (0xff333a63);
        t.panelShadow    = c (0xff05060d);
        t.hairline       = c (0xff272d52);

        t.text      = c (0xffe8ecff);
        t.textDim   = c (0xff9299bd);
        t.textFaint = c (0xff565d85);

        t.accentHot     = c (0xffff3d81);   // magenta — engage
        t.accentHotDim  = c (0xff6b1638);
        t.ember         = c (0xff37e6ff);
        t.emberDim      = c (0xff155f6d);
        t.accentCool    = c (0xff37e6ff);   // ice — running
        t.accentCoolDim = c (0xff155f6d);

        t.knobBody      = c (0xff232a4e);
        t.knobBodyLight = c (0xff3c4573);
        t.knobSkirt     = c (0xff0b0d1c);
        t.knobPointer   = c (0xffe8ecff);
        t.knobArc       = c (0xff272d52);

        t.sliderTrack     = c (0xff0b0d1c);
        t.sliderFill      = c (0xff37e6ff);
        t.buttonFace      = c (0xff1d2242);
        t.buttonFaceHover = c (0xff2a3157);

        t.meterGlass     = c (0xff101a3a);
        t.meterGlassEdge = c (0xff06091a);
        t.meterScale     = c (0xffc9d6ff);
        t.meterNeedle    = c (0xffe8ecff);   // a lit needle on dark glass
        t.meterRedZone   = c (0xffff3d81);
        t.meterPeakLamp  = c (0xffff3d81);

        t.category = {{
            c (0xffff5a4f),   // Kick
            c (0xffffc857),   // Snare
            c (0xffff5fd0),   // Clap
            c (0xff37e6ff),   // HatClosed
            c (0xff1f8fb0),   // HatOpen
            c (0xffa87bff),   // Tom
            c (0xff4dea9e),   // Perc
            c (0xff8790c4),   // Fx
            c (0xff5f6791),   // Unknown
        }};
        return t;
    }

    ThemeId activeId = ThemeId::forge;
}

const Theme& theme() noexcept
{
    // Function-local statics: built on first use, so no static-init-order dependency on
    // JUCE's Colour, and never rebuilt.
    static const Theme forge    = makeForge();
    static const Theme studio   = makeStudio();
    static const Theme midnight = makeMidnight();

    switch (activeId)
    {
        case ThemeId::studio:   return studio;
        case ThemeId::midnight: return midnight;
        case ThemeId::forge:
        default:                return forge;
    }
}

void setTheme (ThemeId id) noexcept
{
    activeId = id;
}

ThemeId themeIdFromString (const juce::String& name) noexcept
{
    const auto n = name.trim().toLowerCase();
    if (n == "studio")   return ThemeId::studio;
    if (n == "midnight") return ThemeId::midnight;
    return ThemeId::forge;
}

const char* themeIdToString (ThemeId id) noexcept
{
    switch (id)
    {
        case ThemeId::studio:   return "studio";
        case ThemeId::midnight: return "midnight";
        case ThemeId::forge:
        default:                return "forge";
    }
}

} // namespace rollforge
