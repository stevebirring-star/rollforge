#include "ui/AboutView.h"
#include "ui/Theme.h"

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
        "New Sounds  -  the mirror of Reroll. Reroll keeps the kit and rewrites the "
        "notes; New Sounds keeps the notes, exactly as they are, and deals a fresh kit "
        "from your library. A groove you like is worth hearing through more than one set "
        "of drums. Needs a scanned library.\n"
        "\n"
        "Lock a lane  -  click the padlock next to a row to keep its steps through "
        "Make a Beat / Reroll / Vary. It protects that lane's SOUND through New Sounds "
        "too: one padlock, one meaning. \"Keep the kick, gamble the rest.\"\n"
        "\n"
        "Pads  -  each pad wears the colour of its sound, on the pad, on its waveform and "
        "on its sequencer lane. Click one to audition it; drag an audio file onto it to "
        "load it. HOVER a pad to reveal its controls: M / S mute or solo it, R reverses "
        "it, and two handles on its bottom edge trim the sample. Anything switched on "
        "stays visible. RIGHT-CLICK a pad for its TONE (tilt it darker or brighter), its "
        "SEND (how much of it feeds the reverb), and SIMILAR. Keys 1234 / QWER / ASDF / "
        "ZXCV trigger the 16 pads; Space plays the kick.\n"
        "\n"
        "Similar  -  in a pad's right-click bubble, swaps that pad for the closest sound "
        "of the SAME kind in your library. Press it again to step to the next-nearest, "
        "and again, walking a shortlist without leaving the grid. It keeps the pad's TONE "
        "and SEND, because you are auditioning the same slot. Closeness is measured on "
        "length, level, brightness, sustain and density -- never across drum types, so a "
        "kick's neighbours are always kicks.\n"
        "\n"
        "Layers  -  drop SEVERAL samples on one pad and it cycles through them, so a "
        "repeated hit never sounds twice the same. Right-click the pad to switch it to "
        "By velocity, where soft hits pick the first sample and hard hits the last.\n"
        "\n"
        "Sequencer  -  click a step to toggle it; drag up or down on a lit step to set "
        "its velocity. All 16 kit pads have a lane.\n"
        "\n"
        "Patterns A-H  -  eight patterns, one kit. Click a letter to go there. While the "
        "transport is running the change lands on the next BAR LINE, not the instant your "
        "finger moved, so a verse becomes a chorus in time; the letter you are waiting for "
        "pulses until it takes. A faint letter is an empty slot. Right-click one to copy "
        "the current pattern into it, or to clear it. Tempo and swing belong to the "
        "transport, not to a slot, so switching never changes them. All eight are saved in "
        "a .rollforge file.\n"
        "\n"
        "Evolve  -  infinity mode: never play the same bar twice. Every bar the engine is "
        "handed a fresh variation of your pattern -- a few hits moved, ghosts added, accents "
        "shifted -- and the changed steps flash. Drift sets how far each bar strays. Crucially "
        "each bar varies THE PATTERN YOU WROTE, not the bar before it: varying a variation is "
        "a random walk, and a random walk turns a groove into mush in a dozen bars. Lock a "
        "lane to hold it still while the rest breathe. Edit a step, or hit Make a Beat, and "
        "that becomes the new home. Switch EVOLVE off and the variation you landed on is "
        "yours to keep.\n"
        "\n"
        "Song  -  chain those eight into an arrangement: A x2, B, A x2, C. ADD appends the "
        "pattern you are standing in; click a step to cycle how many bars it lasts (1, 2, 4, "
        "8); right-click one to remove it. Turn SONG on and the chain drives the switching, "
        "each step landing on its own bar line. The step that is sounding is outlined in "
        "blue. Loop repeats the chain; with Loop off the transport stops when it ends. The "
        "chain is saved with the project.\n"
        "\n"
        "Triplets  -  the \"3\" beside a lane runs it in 1/8-note triplets: twelve steps "
        "to the bar instead of sixteen, so a triplet hat can ride over a straight kick. "
        "Swing and the roll brush are straight-1/16 ideas and don't apply to those lanes.\n"
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
        "Meters  -  the two needles are the left and right master outputs, measured after "
        "the limiter. They are true VU meters: 300 ms averaging, so they read loudness and "
        "ignore single transients. The slim bar on each face is sample peak, and CLIP "
        "lights if the output goes over.\n"
        "\n"
        "Master  -  PUNCH, SPACE, CRUSH, DRIVE shape the master bus; LOW / MID / HIGH "
        "are a 3-band EQ and COMP is a one-knob glue compressor (all flat / 0 = bypass). "
        "SPACE washes the whole mix; a pad's SEND feeds a separate reverb, so you can wet "
        "the snare and leave the kick dry.\n"
        "\n"
        "Library  -  scan a folder of samples, browse, and NEW KIT builds a playable "
        "kit. Click a sample to hear it. Right-click one to send it to a pad, slice it "
        "across the pads, or re-tag its category; the correction sticks and survives a "
        "re-scan. Swapping a pad's sample works while the beat is playing. No sample is "
        "dealt to two pads unless its category has fewer sounds than pads asking for one.\n"
        "\n"
        "Map  -  the same library as a constellation: one dot per sample, placed by how it "
        "sounds and coloured by what it is. Near dots are near sounds -- it is the very "
        "space a pad's SIMILAR button searches. Hover to name a dot, click to hear it, "
        "right-click for the same actions as the list. The captions name whichever quality "
        "each axis turned out to measure. Filtering dims the rest instead of moving "
        "anything, so the shape of your library stays where you left it.\n"
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
    g.fillAll (theme().background);

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
