#include "model/ProjectIO.h"

namespace rollforge
{
namespace ProjectIO
{

namespace
{
    using juce::var;
    using juce::DynamicObject;

    var stepToVar (const Step& s)
    {
        auto* o = new DynamicObject();
        o->setProperty ("on",    s.on);
        o->setProperty ("vel",   s.velocity);
        o->setProperty ("micro", s.microShift);
        o->setProperty ("rat",   s.ratchets);
        o->setProperty ("ramp",  s.ratchetRamp);
        o->setProperty ("prob",  s.probability);
        o->setProperty ("lock",  s.sampleLock);
        return var (o);
    }

    Step stepFromVar (const var& v)
    {
        Step s;
        s.on          = (bool)  v.getProperty ("on",    false);
        s.velocity    = (float) (double) v.getProperty ("vel",   0.8);
        s.microShift  = (float) (double) v.getProperty ("micro", 0.0);
        s.ratchets    = (int)   v.getProperty ("rat",   1);
        s.ratchetRamp = (float) (double) v.getProperty ("ramp",  0.0);
        s.probability = (int)   v.getProperty ("prob",  100);
        s.sampleLock  = (int)   v.getProperty ("lock",  -1);
        return s;
    }

    var patternToVar (const Pattern& p)
    {
        auto* o = new DynamicObject();
        o->setProperty ("numLanes", p.numLanes);
        o->setProperty ("bpm",      p.bpm);
        o->setProperty ("swing",    p.swing);

        juce::Array<var> lanes;
        for (int li = 0; li < p.numLanes && li < maxLanes; ++li)
        {
            const Lane& lane = p.lane (li);
            auto* lo = new DynamicObject();
            lo->setProperty ("pad", lane.targetPad);
            lo->setProperty ("len", lane.length);

            juce::Array<var> steps;
            int len = lane.length;
            if (len < 0) len = 0;
            if (len > maxStepsPerLane) len = maxStepsPerLane;
            for (int s = 0; s < len; ++s)
                steps.add (stepToVar (lane.step (s)));
            lo->setProperty ("steps", steps);
            lanes.add (var (lo));
        }
        o->setProperty ("lanes", lanes);

        juce::Array<var> rolls;
        for (int ri = 0; ri < p.numRolls && ri < maxRolls; ++ri)
        {
            const CompiledRoll& roll = p.rolls[(size_t) ri];
            auto* ro = new DynamicObject();
            ro->setProperty ("pad",   roll.targetPad);
            ro->setProperty ("start", roll.startStep);

            juce::Array<var> evs;
            int n = roll.count;
            if (n < 0) n = 0;
            if (n > maxRollEvents) n = maxRollEvents;
            for (int k = 0; k < n; ++k)
            {
                auto* eo = new DynamicObject();
                eo->setProperty ("off",   roll.events[(size_t) k].stepOffset);
                eo->setProperty ("vel",   roll.events[(size_t) k].velocity);
                eo->setProperty ("pitch", roll.events[(size_t) k].pitchSemitones);
                evs.add (var (eo));
            }
            ro->setProperty ("events", evs);
            rolls.add (var (ro));
        }
        o->setProperty ("rolls", rolls);

        return var (o);
    }

    void patternFromVar (const var& v, Pattern& p)
    {
        p.numLanes = (int) v.getProperty ("numLanes", 0);
        if (p.numLanes < 0) p.numLanes = 0;
        if (p.numLanes > maxLanes) p.numLanes = maxLanes;
        p.bpm   = (double) v.getProperty ("bpm", 120.0);
        p.swing = (float) (double) v.getProperty ("swing", 0.0);

        if (auto* lanes = v.getProperty ("lanes", var()).getArray())
        {
            for (int li = 0; li < lanes->size() && li < maxLanes; ++li)
            {
                const var& lv = (*lanes)[li];
                Lane& lane = p.lane (li);
                lane.targetPad = (int) lv.getProperty ("pad", 0);
                lane.length    = (int) lv.getProperty ("len", 16);

                if (auto* steps = lv.getProperty ("steps", var()).getArray())
                    for (int s = 0; s < steps->size() && s < maxStepsPerLane; ++s)
                        lane.step (s) = stepFromVar ((*steps)[s]);
            }
        }

        if (auto* rolls = v.getProperty ("rolls", var()).getArray())
        {
            const int nr = juce::jmin (rolls->size(), maxRolls);
            p.numRolls = nr;
            for (int ri = 0; ri < nr; ++ri)
            {
                const var& rv = (*rolls)[ri];
                CompiledRoll& roll = p.rolls[(size_t) ri];
                roll.targetPad = (int) rv.getProperty ("pad", 0);
                roll.startStep = (float) (double) rv.getProperty ("start", 0.0);
                roll.count = 0;

                if (auto* evs = rv.getProperty ("events", var()).getArray())
                {
                    const int ne = juce::jmin (evs->size(), maxRollEvents);
                    roll.count = ne;
                    for (int k = 0; k < ne; ++k)
                    {
                        const var& ev = (*evs)[k];
                        roll.events[(size_t) k].stepOffset     = (float) (double) ev.getProperty ("off",   0.0);
                        roll.events[(size_t) k].velocity       = (float) (double) ev.getProperty ("vel",   1.0);
                        roll.events[(size_t) k].pitchSemitones = (float) (double) ev.getProperty ("pitch", 0.0);
                    }
                }
            }
        }
    }
}

juce::String toJson (const Project& proj)
{
    auto* root = new DynamicObject();
    root->setProperty ("version", proj.version);
    root->setProperty ("bpm",     proj.bpm);
    root->setProperty ("swing",   proj.swing);
    root->setProperty ("punch",   proj.punch);
    root->setProperty ("space",   proj.space);
    root->setProperty ("crush",   proj.crush);
    root->setProperty ("drive",   proj.drive);

    juce::Array<var> pads;
    for (const auto& pad : proj.pads)
    {
        auto* po = new DynamicObject();
        po->setProperty ("path",    pad.samplePath);
        po->setProperty ("gain",    pad.gain);
        po->setProperty ("pitch",   pad.pitchSemitones);
        po->setProperty ("pan",     pad.pan);
        po->setProperty ("choke",   pad.chokeGroup);
        po->setProperty ("reverse", pad.reverse);
        pads.add (var (po));
    }
    root->setProperty ("pads", pads);
    root->setProperty ("pattern", patternToVar (proj.pattern));

    return juce::JSON::toString (var (root));
}

bool fromJson (const juce::String& json, Project& out)
{
    const var root = juce::JSON::parse (json);
    if (! root.isObject())
        return false;

    out = Project {};
    out.version = (int) root.getProperty ("version", 1);
    out.bpm     = (double) root.getProperty ("bpm", 120.0);
    out.swing   = (float) (double) root.getProperty ("swing", 0.0);
    out.punch   = (float) (double) root.getProperty ("punch", 0.0);
    out.space   = (float) (double) root.getProperty ("space", 0.0);
    out.crush   = (float) (double) root.getProperty ("crush", 0.0);
    out.drive   = (float) (double) root.getProperty ("drive", 0.0);

    if (auto* pads = root.getProperty ("pads", var()).getArray())
    {
        for (int i = 0; i < pads->size() && i < projectNumPads; ++i)
        {
            const var& pv = (*pads)[i];
            ProjectPad& pad = out.pads[(size_t) i];
            pad.samplePath     = pv.getProperty ("path", "").toString();
            pad.gain           = (float) (double) pv.getProperty ("gain", 1.0);
            pad.pitchSemitones = (float) (double) pv.getProperty ("pitch", 0.0);
            pad.pan            = (float) (double) pv.getProperty ("pan", 0.0);
            pad.chokeGroup     = (int) pv.getProperty ("choke", 0);
            pad.reverse        = (bool) pv.getProperty ("reverse", false);
        }
    }

    patternFromVar (root.getProperty ("pattern", var()), out.pattern);
    return true;
}

bool save (const Project& proj, const juce::File& file)
{
    return file.replaceWithText (toJson (proj));
}

bool load (const juce::File& file, Project& out)
{
    if (! file.existsAsFile())
        return false;
    return fromJson (file.loadFileAsString(), out);
}

} // namespace ProjectIO
} // namespace rollforge
