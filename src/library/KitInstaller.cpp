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

} // namespace rollforge
