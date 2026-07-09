#include "ui/AboutView.h"

namespace rollforge
{

AboutView::AboutView()
{
    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setCaretVisible (false);
    body.setScrollbarsShown (true);
    body.setPopupMenuEnabled (false);
    body.setFont (juce::FontOptions (13.5f));
    body.setColour (juce::TextEditor::backgroundColourId,     juce::Colours::transparentBlack);
    body.setColour (juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
    body.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    body.setColour (juce::TextEditor::textColourId,           juce::Colour (0xffc4c4cc));

    // KEEP IN SYNC WITH THE UI: this quick-start is the app's only in-product help.
    // When you add, remove or rename a user-facing feature, update the matching line
    // here in the same change. One line per top-level feature, in on-screen order.
    body.setText (
        "Getting started\n"
        "\n"
        "Make a Beat  -  one tap generates a full groove in the chosen style. Reroll "
        "for a fresh take; the Style box and the 1-5 slider set the flavour and density.\n"
        "\n"
        "Vary  -  tweak the CURRENT beat instead of regenerating it: a few hits move, "
        "ghost notes appear and accents shift. The steps it changed flash amber. The "
        "1-5 slider sets how much it varies.\n"
        "\n"
        "Lock a lane  -  click the padlock next to a row to keep its steps through "
        "Make a Beat / Reroll / Vary. \"Keep the kick, gamble the rest.\"\n"
        "\n"
        "Pads  -  click a pad to audition it; drag an audio file onto a pad to load it. "
        "M / S mute or solo a pad, R reverses it; drag the two handles along a pad's "
        "bottom edge to trim the sample. Keys 1234 / QWER / ASDF / ZXCV trigger the 16 "
        "pads; Space plays the kick.\n"
        "\n"
        "Sequencer  -  click a step to toggle it; drag up or down on a lit step to set "
        "its velocity. All 16 kit pads have a lane.\n"
        "\n"
        "Tempo & feel  -  set BPM directly or tap it; Swing pushes the offbeats; the "
        "Feel box (Straight / Human / Boom-bap loose / Trap tight / Drunk) loosens "
        "timing, velocity and swing together so beats don't sound robotic.\n"
        "\n"
        "Roll Brush  -  toggle it on, then drag across a lane to paint an accelerating "
        "roll (drag up = denser). The preset box shapes the roll; Clear Rolls removes them.\n"
        "\n"
        "Note Repeat  -  turn it on, then HOLD a pad to retrigger it at the chosen rate "
        "(1/8 to 1/32). \"Build\" accelerates and crescendos into a live roll.\n"
        "\n"
        "Master  -  PUNCH, SPACE, CRUSH, DRIVE shape the master bus; LOW / MID / HIGH "
        "are a 3-band EQ and COMP is a one-knob glue compressor (all flat / 0 = bypass).\n"
        "\n"
        "Library  -  scan a folder of samples, browse, and NEW KIT builds a playable "
        "kit. Click a sample to hear it. Right-click one to send it to a pad, slice it "
        "across the pads, or re-tag its category; the correction sticks and survives a "
        "re-scan. Swapping a pad's sample works while the beat is playing.\n"
        "\n"
        "Save / Open  -  Save keeps your whole session (kit, pattern, rolls, FX, "
        "mute/solo, tempo) as a .rollforge file; Open loads one back.\n"
        "\n"
        "Slice  -  chop a drum loop at its onsets, one slice per pad, and lay the break "
        "back out on the grid. The tempo is set so one bar lasts exactly as long as the "
        "loop, so pressing Play replays the break. One Ctrl/Cmd+Z undoes the whole thing.\n"
        "\n"
        "Export  -  render MIDI, a WAV mix, or per-pad stems from the Export dialog. The "
        "Length selector renders 1, 2, 4 or 8 loops. Stems are pre-master, so they add "
        "back up to the mix. Or grab the \"Drag MIDI\" / \"Drag Audio\" chips and drag "
        "the loop straight into your DAW.\n"
        "\n"
        "Settings  -  choose the audio device and buffer size, the UI scale, and which "
        "folders to scan for samples.\n"
        "\n"
        "Undo / redo  -  Ctrl/Cmd+Z, and Shift+Ctrl/Cmd+Z to redo.\n",
        false);
    addAndMakeVisible (body);

    setSize (480, 500);
}

void AboutView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1e));

    auto r = getLocalBounds().reduced (22, 18);
    auto header = r.removeFromTop (58);

    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("RollForge", header.removeFromTop (34), juce::Justification::topLeft, false);

    g.setColour (juce::Colour (0xff9a9aa4));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("version " JUCE_APPLICATION_VERSION_STRING "   -   the fast, non-technical beat sketchpad",
                header, juce::Justification::topLeft, false);

    // Divider under the header.
    g.setColour (juce::Colour (0xff31313a));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1);
}

void AboutView::resized()
{
    auto r = getLocalBounds().reduced (22, 18);
    r.removeFromTop (58 + 12);   // header block + divider gap
    body.setBounds (r);
}

} // namespace rollforge
