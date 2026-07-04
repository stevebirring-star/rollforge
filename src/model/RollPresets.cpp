#include "model/RollPresets.h"

namespace rollforge
{
namespace RollPresets
{

const char* name (Preset preset) noexcept
{
    switch (preset)
    {
        case TrapTriplet: return "Trap Triplet";
        case Buildup:     return "Buildup";
        case Stutter:     return "Stutter";
        case DrillSlide:  return "Drill Slide";
        case MachineGun:  return "Machine Gun";
        case FadeRoll:    return "Fade Roll";
        case Crescendo:   return "Crescendo";
        case Decelerate:  return "Decelerate";
        case PitchRise:   return "Pitch Rise";
        case Ramp32:      return "1/32 Ramp";
        default:          return "Roll";
    }
}

RollRegion make (Preset preset, double startStep, double lengthSteps, int targetPad)
{
    RollRegion r;
    r.startStep   = startStep;
    r.lengthSteps = lengthSteps;
    r.targetPad   = targetPad;

    switch (preset)
    {
        case TrapTriplet: r.speed = { 3.0f, 3.0f,  0.0f }; r.volume = { 1.0f, 0.85f, 0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case Buildup:     r.speed = { 2.0f, 8.0f,  0.5f }; r.volume = { 0.6f, 1.0f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case Stutter:     r.speed = { 6.0f, 6.0f,  0.0f }; r.volume = { 1.0f, 1.0f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case DrillSlide:  r.speed = { 2.0f, 6.0f,  0.3f }; r.volume = { 1.0f, 0.7f,  0.0f }; r.pitch = { 0.0f, -5.0f, 0.0f }; break;
        case MachineGun:  r.speed = { 8.0f, 16.0f, 0.0f }; r.volume = { 1.0f, 0.9f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case FadeRoll:    r.speed = { 4.0f, 4.0f,  0.0f }; r.volume = { 1.0f, 0.1f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case Crescendo:   r.speed = { 4.0f, 4.0f,  0.0f }; r.volume = { 0.3f, 1.0f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case Decelerate:  r.speed = { 8.0f, 2.0f, -0.3f }; r.volume = { 1.0f, 0.8f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        case PitchRise:   r.speed = { 4.0f, 4.0f,  0.0f }; r.volume = { 1.0f, 1.0f,  0.0f }; r.pitch = { 0.0f, 12.0f, 0.0f }; break;
        case Ramp32:      r.speed = { 8.0f, 8.0f,  0.0f }; r.volume = { 1.0f, 0.8f,  0.0f }; r.pitch = { 0.0f,  0.0f, 0.0f }; break;
        default: break;
    }

    return r;
}

} // namespace RollPresets
} // namespace rollforge
