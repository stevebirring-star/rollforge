#include "engine/Sequencer.h"

namespace rollforge
{

Sequencer::Sequencer()
{
    // Publish an empty pattern so read() always returns a valid one.
    patterns.writeBuffer() = Pattern {};
    patterns.publish();
}

void Sequencer::prepare (double sampleRate) noexcept
{
    clock.prepare (sampleRate);
    currentStep.store (-1, std::memory_order_release);
}

void Sequencer::setPattern (const Pattern& pattern)
{
    patterns.writeBuffer() = pattern;
    patterns.publish();
}

void Sequencer::setTempo (double bpm) noexcept
{
    pendingTempo.store (bpm, std::memory_order_release);
    tempoDirty.store (true, std::memory_order_release);
}

void Sequencer::process (DrumEngine& engine, juce::AudioBuffer<float>& buffer) noexcept
{
    // Apply transport control marshaled from the message thread.
    if (tempoDirty.exchange (false, std::memory_order_acquire))
        clock.setTempo (pendingTempo.load (std::memory_order_relaxed));
    if (resetRequested.exchange (false, std::memory_order_acquire))
        clock.reset();
    clock.setPlaying (playing.load (std::memory_order_acquire));

    const int numSamples = buffer.getNumSamples();

    // UI + MIDI queued triggers land at the block start.
    engine.drainCommands();

    const Pattern& pattern = patterns.read();

    int cursor = 0;
    clock.processBlock (numSamples, [&] (std::int64_t stepIndex, int offset) noexcept
    {
        if (offset > cursor)
        {
            engine.renderInto (buffer, cursor, offset - cursor);
            cursor = offset;
        }
        fireStep (engine, pattern, stepIndex);
        currentStep.store (stepIndex, std::memory_order_release);
    });

    if (cursor < numSamples)
        engine.renderInto (buffer, cursor, numSamples - cursor);
}

void Sequencer::fireStep (DrumEngine& engine, const Pattern& pattern, std::int64_t stepIndex) noexcept
{
    int lanes = pattern.numLanes;
    if (lanes < 0)        lanes = 0;
    if (lanes > maxLanes) lanes = maxLanes;

    for (int li = 0; li < lanes; ++li)
    {
        const Lane& lane = pattern.lane (li);

        int len = lane.length;
        if (len < 1) continue;
        if (len > maxStepsPerLane) len = maxStepsPerLane;

        const int pos = (int) (stepIndex % (std::int64_t) len);
        const Step& s = lane.step (pos);

        if (s.on)
        {
            engine.triggerPadNow (lane.targetPad, s.velocity);
            triggerCount.fetch_add (1, std::memory_order_acq_rel);
        }
    }
}

} // namespace rollforge
