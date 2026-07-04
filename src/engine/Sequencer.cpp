#include "engine/Sequencer.h"

#include "model/Humaniser.h"

#include <cmath>

namespace rollforge
{

namespace
{
    template <typename T>
    T clampVal (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    // Deterministic hash of (stepIndex, laneIndex) -> [0, 1). Same pattern loops
    // reproducibly; different absolute steps (i.e. later bars) get different values.
    float hashUnitFloat (std::int64_t a, std::int64_t b) noexcept
    {
        std::uint64_t x = (std::uint64_t) a * 0x9E3779B97F4A7C15ull
                        + (std::uint64_t) b * 0xBF58476D1CE4E5B9ull
                        + 0xD6E8FEB86659FD93ull;
        x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
        x ^= x >> 27; x *= 0x94D049BB133111EBull;
        x ^= x >> 31;
        return (float) ((double) (x >> 40) / (double) (1u << 24));   // 24-bit -> [0,1)
    }
}

Sequencer::Sequencer()
    : incoming (std::make_unique<TripleBuffer<Pattern>>()),
      active   (std::make_unique<Pattern>()),
      queued   (std::make_unique<Pattern>())
{
    pending.resize (maxPendingEvents);
}

void Sequencer::prepare (double sampleRate) noexcept
{
    clock.prepare (sampleRate);
    pendingCount = 0;
    currentStep.store (-1, std::memory_order_release);
}

void Sequencer::setPattern (const Pattern& pattern)
{
    incoming->writeBuffer() = pattern;
    incoming->publish();
    incomingMode.store (1, std::memory_order_release);   // immediate
}

void Sequencer::queuePattern (const Pattern& pattern)
{
    incoming->writeBuffer() = pattern;
    incoming->publish();
    incomingMode.store (2, std::memory_order_release);   // swap at the next bar
}

void Sequencer::setTempo (double bpm) noexcept
{
    pendingTempo.store (bpm, std::memory_order_release);
    tempoDirty.store (true, std::memory_order_release);
}

void Sequencer::applyIncomingPattern (bool nowPlaying) noexcept
{
    const int mode = incomingMode.exchange (0, std::memory_order_acquire);
    if (mode == 1)
    {
        *active = incoming->read();        // immediate replace
        hasQueued = false;
        switchQueued.store (false, std::memory_order_release);
    }
    else if (mode == 2)
    {
        *queued = incoming->read();        // stash until the bar boundary
        hasQueued = true;
        switchQueued.store (true, std::memory_order_release);
    }

    // Stopped: nothing to wait for, so apply a queued switch immediately.
    if (! nowPlaying && hasQueued)
    {
        *active = *queued;
        hasQueued = false;
        switchQueued.store (false, std::memory_order_release);
    }
}

void Sequencer::process (DrumEngine& engine, juce::AudioBuffer<float>& buffer) noexcept
{
    // Transport control marshaled from the message thread.
    if (tempoDirty.exchange (false, std::memory_order_acquire))
        clock.setTempo (pendingTempo.load (std::memory_order_relaxed));
    if (resetRequested.exchange (false, std::memory_order_acquire))
    {
        clock.reset();
        pendingCount = 0;
    }

    const bool nowPlaying = playing.load (std::memory_order_acquire);
    applyIncomingPattern (nowPlaying);
    clock.setPlaying (nowPlaying);

    const int numSamples = buffer.getNumSamples();

    // UI + MIDI queued triggers land at the block start.
    engine.drainCommands();

    if (! nowPlaying)
    {
        // Transport paused: just render decaying voices; don't advance events.
        engine.renderInto (buffer, 0, numSamples);
        return;
    }

    const std::int64_t blockStart = clock.getSampleCounter();

    // 1. Generate this block's step events (swapping a queued pattern on the bar).
    clock.processBlock (numSamples, [&] (std::int64_t stepIndex, int offset) noexcept
    {
        if (hasQueued && (stepIndex % barLengthSteps) == 0)
        {
            *active = *queued;
            hasQueued = false;
            switchQueued.store (false, std::memory_order_release);
        }

        generateStepEvents (stepIndex, blockStart + (std::int64_t) offset);
        currentStep.store (stepIndex, std::memory_order_release);
    });

    // 2. Fire pending events landing in this block, in order, splitting segments.
    renderWithEvents (engine, buffer, blockStart, numSamples);
}

void Sequencer::generateStepEvents (std::int64_t stepIndex, std::int64_t stepSample) noexcept
{
    int lanes = active->numLanes;
    if (lanes < 0)        lanes = 0;
    if (lanes > maxLanes) lanes = maxLanes;

    const double samplesPerStep = clock.getSamplesPerStep();
    const double swingAmount    = clampVal ((double) swing.load (std::memory_order_relaxed), 0.0, 1.0);
    const float  humaniseAmt    = clampVal ((float) humanise.load (std::memory_order_relaxed), 0.0f, 1.0f);

    for (int li = 0; li < lanes; ++li)
    {
        const Lane& lane = active->lane (li);

        int len = lane.length;
        if (len < 1) continue;
        if (len > maxStepsPerLane) len = maxStepsPerLane;

        const int   pos = (int) (stepIndex % (std::int64_t) len);
        const Step& s   = lane.step (pos);
        if (! s.on)
            continue;

        // Probability gate (deterministic per absolute step + lane).
        if (s.probability < 100)
        {
            if (s.probability <= 0)
                continue;
            if (hashUnitFloat (stepIndex, li) >= (float) s.probability / 100.0f)
                continue;
        }

        // Forward timing offset = micro-shift + swing, both applied LATER only.
        // Micro-shift is forward-only for now (a backward shift would land before
        // the block that generates the step -> wrong + buffer-size dependent; a
        // negative value is clamped to 0). Swing delays the off-beat (odd) 1/16s.
        double forward = clampVal ((double) s.microShift, 0.0, 0.5);
        if ((stepIndex & 1) == 1)
            forward += swingAmount / 3.0;   // swing 1 -> ~66:33 shuffle
        forward += (double) Humaniser::timingSteps (Humaniser::eventHash (stepIndex, li, 0), humaniseAmt);
        const std::int64_t base = stepSample + (std::int64_t) std::llround (forward * samplesPerStep);

        // Humanised base velocity (seeded, non-destructive) — shared by all ratchets.
        const float stepVelocity = clampVal (s.velocity
            + Humaniser::velocityDelta (Humaniser::eventHash (stepIndex, li, 1), humaniseAmt), 0.0f, 1.0f);

        // Ratchets: `r` evenly-spaced sub-hits across the step, velocity-ramped.
        const int r = clampVal (s.ratchets, 1, 8);
        for (int j = 0; j < r; ++j)
        {
            const std::int64_t evSample = base + (std::int64_t) std::llround ((double) j * samplesPerStep / (double) r);

            float velocity = stepVelocity;
            if (r > 1)
            {
                const float t = (float) j / (float) (r - 1);      // 0..1
                velocity *= 1.0f + s.ratchetRamp * (t - 0.5f);     // ramp around the base
            }
            velocity = clampVal (velocity, 0.0f, 1.0f);

            addEvent (evSample, lane.targetPad, velocity);
        }
    }

    // Rolls: fire any roll whose start step matches this step's position in the bar.
    // The roll's events are pre-compiled (step offsets); we just scale to samples.
    const int posInBar = (int) (((stepIndex % barLengthSteps) + barLengthSteps) % barLengthSteps);
    int rolls = active->numRolls;
    if (rolls < 0)        rolls = 0;
    if (rolls > maxRolls) rolls = maxRolls;

    for (int ri = 0; ri < rolls; ++ri)
    {
        const CompiledRoll& roll = active->rolls[(size_t) ri];
        if ((int) std::lround (roll.startStep) != posInBar)
            continue;

        int n = roll.count;
        if (n > maxRollEvents) n = maxRollEvents;
        for (int k = 0; k < n; ++k)
        {
            const RollEvent& e = roll.events[(size_t) k];
            const std::int64_t evSample = stepSample
                + (std::int64_t) std::llround ((double) e.stepOffset * samplesPerStep);
            addEvent (evSample, roll.targetPad, e.velocity, e.pitchSemitones);
        }
    }
}

void Sequencer::addEvent (std::int64_t sample, int pad, float velocity, float pitchOffset) noexcept
{
    if (pendingCount < maxPendingEvents)
        pending[pendingCount++] = { sample, pad, velocity, pitchOffset };
    // else: buffer full (pathological ratchet/lane/roll count) -> drop this event.
}

void Sequencer::renderWithEvents (DrumEngine& engine, juce::AudioBuffer<float>& buffer,
                                  std::int64_t blockStart, int numSamples) noexcept
{
    const std::int64_t blockEnd = blockStart + numSamples;
    int cursor = 0;

    // Fire pending events with sample < blockEnd, in ascending order, splitting
    // render segments at each. O(n^2) over a small bounded buffer -> RT-safe.
    while (true)
    {
        int          best       = -1;
        std::int64_t bestSample  = blockEnd;
        for (int i = 0; i < pendingCount; ++i)
            if (pending[i].sample < bestSample)
            {
                bestSample = pending[i].sample;
                best = i;
            }

        if (best < 0)
            break;   // nothing left before blockEnd

        int offset = (int) (pending[best].sample - blockStart);
        offset = clampVal (offset, cursor, numSamples);   // clamp past events into the block

        if (offset > cursor)
        {
            engine.renderInto (buffer, cursor, offset - cursor);
            cursor = offset;
        }

        engine.triggerPadNow (pending[best].pad, pending[best].velocity, pending[best].pitchOffset);
        triggerCount.fetch_add (1, std::memory_order_acq_rel);

        pending[best] = pending[--pendingCount];   // remove (swap with last)
    }

    if (cursor < numSamples)
        engine.renderInto (buffer, cursor, numSamples - cursor);
}

} // namespace rollforge
