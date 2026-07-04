#include "model/MidiExporter.h"

#include <cmath>
#include <memory>

namespace rollforge
{
namespace MidiExporter
{

namespace
{
    constexpr int barSteps    = 16;
    constexpr int drumChannel = 10;   // GM drums

    // A GM-ish drum map for the 4x4 pad grid.
    const int kGmMap[16] = {
        36, 38, 42, 46,   // kick, snare, closed hat, open hat
        39, 45, 37, 49,   // clap, low tom, rimshot, crash
        41, 43, 47, 50,   // toms
        51, 55, 57, 59    // ride, splash, crash2, ride2
    };

    int clampi (int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    void addNote (juce::MidiMessageSequence& seq, int note, float velocity, int tick, int lengthTicks)
    {
        const int v = clampi ((int) std::lround (velocity * 127.0f), 1, 127);
        seq.addEvent (juce::MidiMessage::noteOn  (drumChannel, note, (juce::uint8) v), (double) tick);
        seq.addEvent (juce::MidiMessage::noteOff (drumChannel, note), (double) (tick + lengthTicks));
    }
}

int gmNoteForPad (int pad) noexcept
{
    if (pad < 0 || pad >= 16)
        return 38;
    return kGmMap[pad];
}

juce::MidiMessageSequence toSequence (const Pattern& pattern, int bars, int ticksPerQuarter)
{
    juce::MidiMessageSequence seq;

    const int stepTicks = ticksPerQuarter / 4;   // one 1/16 step
    if (stepTicks <= 0)
        return seq;

    const int lanes      = clampi (pattern.numLanes, 0, maxLanes);
    const int rolls      = clampi (pattern.numRolls, 0, maxRolls);
    const int totalSteps = juce::jmax (0, bars) * barSteps;

    for (int g = 0; g < totalSteps; ++g)
    {
        for (int li = 0; li < lanes; ++li)
        {
            const Lane& lane = pattern.lane (li);
            const int len = clampi (lane.length, 1, maxStepsPerLane);
            const Step& st = lane.step (g % len);
            if (! st.on)
                continue;

            const int   note = gmNoteForPad (lane.targetPad);
            const double fwd  = st.microShift < 0.0f ? 0.0 : (st.microShift > 0.5f ? 0.5 : (double) st.microShift);
            const int   r    = clampi (st.ratchets, 1, 8);

            for (int j = 0; j < r; ++j)
            {
                const double subPos = (double) g + fwd + (double) j / (double) r;
                const int    tick   = (int) std::llround (subPos * stepTicks);

                float vel = st.velocity;
                if (r > 1)
                {
                    const float t = (float) j / (float) (r - 1);
                    vel *= 1.0f + st.ratchetRamp * (t - 0.5f);
                }
                addNote (seq, note, vel, tick, stepTicks / 2);
            }
        }

        const int posInBar = g % barSteps;
        for (int ri = 0; ri < rolls; ++ri)
        {
            const CompiledRoll& roll = pattern.rolls[(size_t) ri];
            if ((int) std::lround (roll.startStep) != posInBar)
                continue;

            const int note = gmNoteForPad (roll.targetPad);
            const int n = clampi (roll.count, 0, maxRollEvents);
            for (int k = 0; k < n; ++k)
            {
                const double pos  = (double) g + (double) roll.events[(size_t) k].stepOffset;
                const int    tick = (int) std::llround (pos * stepTicks);
                addNote (seq, note, roll.events[(size_t) k].velocity, tick, stepTicks / 4);
            }
        }
    }

    seq.updateMatchedPairs();
    return seq;
}

bool save (const Pattern& pattern, const juce::File& file, int bars)
{
    constexpr int tpq = 960;

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence tempoTrack;
    const double bpm = pattern.bpm > 0.0 ? pattern.bpm : 120.0;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)));
    mf.addTrack (tempoTrack);

    mf.addTrack (toSequence (pattern, bars, tpq));

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr)
        return false;
    return mf.writeTo (*os);
}

} // namespace MidiExporter
} // namespace rollforge
