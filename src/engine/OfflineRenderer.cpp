#include "engine/OfflineRenderer.h"

#include "engine/MasterBus.h"
#include "engine/Sequencer.h"

#include <cmath>

namespace rollforge
{
namespace OfflineRenderer
{

int render (DrumEngine& engine, const Pattern& pattern, juce::AudioBuffer<float>& out, const Options& opts)
{
    // The same reason as the live callback: the reverbs, the limiter and the compressor
    // all decay into denormals, and an export is thousands of blocks of exactly that.
    const juce::ScopedNoDenormals noDenormals;

    const double sr        = opts.sampleRate > 0.0 ? opts.sampleRate : 44100.0;
    const int    blockSize = opts.blockSize > 0 ? opts.blockSize : 512;
    const double bpm       = pattern.bpm > 0.0 ? pattern.bpm : 120.0;

    engine.prepare (sr, blockSize);

    Sequencer seq;                 // heap-allocates its large pattern state internally
    seq.prepare (sr);
    seq.setTempo (bpm);
    seq.setPattern (pattern);
    seq.setPlaying (true);

    MasterBus bus;
    bus.prepare (sr, blockSize);
    if (opts.applyMasterFx)
    {
        bus.setPunch (opts.punch);
        bus.setDrive (opts.drive);
        bus.setCrush (opts.crush);
        bus.setSpace (opts.space);
        bus.setLowEqDb  (opts.lowEq);
        bus.setMidEqDb  (opts.midEq);
        bus.setHighEqDb (opts.highEq);
        bus.setComp     (opts.comp);
    }

    // 1/16 step = a quarter-note / 4.
    const double samplesPerStep = (sr * 60.0 / bpm) / 4.0;
    const int patternSamples = (int) std::llround ((double) juce::jmax (0, opts.bars) * 16.0 * samplesPerStep);
    const int tailSamples    = (int) std::llround (juce::jmax (0.0, opts.tailSeconds) * sr);
    const int total = juce::jmax (1, patternSamples + tailSamples);

    out.setSize (2, total, false, false, true);
    out.clear();

    int done = 0;
    while (done < total)
    {
        const int n = juce::jmin (blockSize, total - done);
        juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, done, n);
        block.clear();
        seq.process (engine, block);
        if (opts.applyMasterFx)
            bus.process (block);
        done += n;
    }

    return total;
}

} // namespace OfflineRenderer
} // namespace rollforge
