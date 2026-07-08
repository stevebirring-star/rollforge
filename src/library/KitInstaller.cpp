#include "library/KitInstaller.h"

namespace rollforge
{

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
    return vp;
}

void installKitIntoEngine (const Kit& kit, DrumEngine& engine)
{
    for (int i = 0; i < kitNumPads; ++i)
    {
        const Pad& pad = kit.pad (i);
        engine.pushSetPad (i, pad.primarySample(), toVoiceParameters (pad), pad.chokeGroup);
    }
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

    // Retire the OLD buffer BEFORE the Kit drops its reference, so the pool holds
    // it while voices may still be playing it (see SampleBuffer.h contract).
    retirementPool.retire (pad.primarySample());

    pad.alternates[0] = newSample;
    engine.pushSetPad (padIndex, newSample, toVoiceParameters (pad), pad.chokeGroup);
}

void updatePadParamsInEngine (const Kit& kit, DrumEngine& engine, int padIndex)
{
    if (! Kit::isValidIndex (padIndex))
        return;
    const Pad& pad = kit.pad (padIndex);
    if (pad.primarySample() == nullptr)
        return;
    // Same sample, new params -> no retirement needed (the buffer is unchanged).
    engine.pushSetPad (padIndex, pad.primarySample(), toVoiceParameters (pad), pad.chokeGroup);
}

} // namespace rollforge
