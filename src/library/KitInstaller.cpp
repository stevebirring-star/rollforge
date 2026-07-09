#include "library/KitInstaller.h"

namespace rollforge
{

static_assert (maxSampleAlternates == maxPadLayers,
               "The model's per-pad sample slots and the engine's layer slots must match: "
               "installPadIntoEngine copies one array onto the other.");

VoiceParameters toVoiceParameters (const Pad& pad)
{
    VoiceParameters vp;
    vp.gain           = pad.volume;
    vp.pan            = pad.pan;
    vp.pitchSemitones = pad.pitchSemis;
    vp.attackMs       = pad.attackMs;
    vp.releaseMs      = pad.releaseMs;
    vp.reverse        = pad.reverse;
    vp.startFraction  = pad.startFraction;
    vp.endFraction    = pad.endFraction;
    vp.tone           = pad.tone;
    vp.reverbSend     = pad.reverbSend;
    return vp;
}

namespace
{
    /** Pushes ALL of a pad's layers (nulls skipped) with its params + choke + mode. */
    void pushPad (const Pad& pad, DrumEngine& engine, int padIndex)
    {
        engine.pushSetPadLayers (padIndex, pad.alternates.data(), maxSampleAlternates,
                                 pad.layerMode, toVoiceParameters (pad), pad.chokeGroup);
    }

    /** Retires every layer a pad currently holds, so a voice mid-way through any of
        them keeps it alive and the final delete happens on the message thread. */
    void retireAllLayers (SampleRetirementPool& retirementPool, Pad& pad)
    {
        for (auto& layer : pad.alternates)
            retirementPool.retire (layer);
    }
}

void installKitIntoEngine (const Kit& kit, DrumEngine& engine)
{
    for (int i = 0; i < kitNumPads; ++i)
        pushPad (kit.pad (i), engine, i);
}

void installSampleIntoPad (SampleRetirementPool& retirementPool,
                           Kit& kit,
                           DrumEngine& engine,
                           int padIndex,
                           SampleBuffer::Ptr newSample)
{
    if (! Kit::isValidIndex (padIndex) || newSample == nullptr)
        return;

    Pad& pad = kit.pad (padIndex);

    // Retire the OLD buffers BEFORE the Kit drops its references, so the pool holds
    // them while voices may still be playing them (see SampleBuffer.h contract).
    retireAllLayers (retirementPool, pad);

    // One sample means one layer: loading a sound onto a pad replaces the whole stack,
    // it doesn't leave old alternates behind for the round-robin to play.
    pad.alternates = {};
    pad.alternates[0] = newSample;
    pushPad (pad, engine, padIndex);
}

void installLayersIntoPad (SampleRetirementPool& retirementPool,
                           Kit& kit,
                           DrumEngine& engine,
                           int padIndex,
                           const SampleBuffer::Ptr* newLayers,
                           int numLayers)
{
    if (! Kit::isValidIndex (padIndex) || newLayers == nullptr || numLayers <= 0)
        return;

    Pad& pad = kit.pad (padIndex);
    retireAllLayers (retirementPool, pad);

    pad.alternates = {};
    int slot = 0;
    for (int i = 0; i < numLayers && slot < maxSampleAlternates; ++i)
        if (newLayers[i] != nullptr)
            pad.alternates[(std::size_t) slot++] = newLayers[i];

    if (slot == 0)
        return;   // nothing usable; the pad is now empty and falls back to the blip

    pushPad (pad, engine, padIndex);
}

void updatePadParamsInEngine (const Kit& kit, DrumEngine& engine, int padIndex)
{
    if (! Kit::isValidIndex (padIndex))
        return;
    const Pad& pad = kit.pad (padIndex);
    if (pad.primarySample() == nullptr)
        return;
    // Same samples, new params -> no retirement needed (the buffers are unchanged).
    pushPad (pad, engine, padIndex);
}

} // namespace rollforge
