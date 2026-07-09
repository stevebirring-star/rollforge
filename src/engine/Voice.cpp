#include "engine/Voice.h"

#include <cmath>
#include <utility>

namespace rollforge
{

void Voice::prepare (double newSampleRate) noexcept
{
    deviceSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    stop();
}

void Voice::stop() noexcept
{
    active       = false;
    sample       = nullptr;   // decref only (never the last ref on the audio thread)
    sourcePos    = 0.0;
    framesPlayed = 0;
    framesTotal  = 0;
    lastEnv      = 0.0f;
}

void Voice::start (SampleBuffer::Ptr newSample, const Parameters& params, float velocity) noexcept
{
    if (newSample == nullptr || newSample->getNumSamples() <= 0)
    {
        stop();
        return;
    }

    sample = newSample;

    const int    length     = sample->getNumSamples();
    const double baseInc    = sample->getSampleRate() / deviceSampleRate;   // native -> device
    const double pitchRatio = std::pow (2.0, (double) params.pitchSemitones / 12.0);
    const double speed      = baseInc * pitchRatio;                          // magnitude of the step

    // Trim: play only the [start, end] fraction of the sample.
    const double span = (double) (length - 1);
    float s0 = juce::jlimit (0.0f, 1.0f, params.startFraction);
    float s1 = juce::jlimit (0.0f, 1.0f, params.endFraction);
    if (s1 < s0) std::swap (s0, s1);
    double startSrc = (double) s0 * span;
    double endSrc   = (double) s1 * span;
    if (endSrc - startSrc < 1.0)                       // keep at least one source sample
        endSrc = juce::jmin (span, startSrc + 1.0);

    if (params.reverse)
    {
        increment = -speed;
        sourcePos = endSrc;
    }
    else
    {
        increment = speed;
        sourcePos = startSrc;
    }

    const double trimSpan = endSrc - startSrc;
    framesTotal  = speed > 0.0 ? (int) std::floor (trimSpan / speed) + 1 : 1;
    framesPlayed = 0;

    attackFrames  = juce::jlimit (0, framesTotal, (int) (params.attackMs  * 0.001 * deviceSampleRate));
    releaseFrames = juce::jlimit (0, framesTotal, (int) (params.releaseMs * 0.001 * deviceSampleRate));

    levelGain = params.gain * juce::jlimit (0.0f, 1.0f, velocity);
    sendGain  = juce::jlimit (0.0f, 1.0f, params.reverbSend);

    // A Voice is a one-shot, so the tilt starts from clean state on every note.
    toneFilter.setTone (deviceSampleRate, params.tone);

    // Equal-power pan: pan[-1,+1] -> angle[0, pi/2].
    const double angle = ((double) juce::jlimit (-1.0f, 1.0f, params.pan) + 1.0)
                             * 0.25 * juce::MathConstants<double>::pi;
    leftGain  = (float) std::cos (angle);
    rightGain = (float) std::sin (angle);
    monoGain  = 0.5f * (leftGain + rightGain);

    lastEnv = envelopeAt (0);
    active  = true;
}

float Voice::envelopeAt (int frame) const noexcept
{
    float env = 1.0f;

    if (attackFrames > 0 && frame < attackFrames)
        env = (float) frame / (float) attackFrames;

    if (releaseFrames > 0)
    {
        const int releaseStart = framesTotal - releaseFrames;
        if (frame >= releaseStart)
            env = juce::jmin (env, (float) (framesTotal - frame) / (float) releaseFrames);
    }

    return juce::jlimit (0.0f, 1.0f, env);
}

float Voice::readMono (double pos) const noexcept
{
    const int   length = sample->getNumSamples();
    const int   i0     = (int) std::floor (pos);
    const float frac   = (float) (pos - (double) i0);

    auto monoAt = [this, length] (int k) -> float
    {
        if (k < 0 || k >= length)
            return 0.0f;

        const int numChannels = sample->getNumChannels();
        if (numChannels <= 0)
            return 0.0f;

        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sum += sample->getSample (ch, k);

        return sum / (float) numChannels;
    };

    return (1.0f - frac) * monoAt (i0) + frac * monoAt (i0 + 1);
}

void Voice::renderAdditive (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                           float* sendOut) noexcept
{
    if (! active || sample == nullptr)
        return;

    const int  outChannels = buffer.getNumChannels();
    const bool feedsSend   = sendOut != nullptr && sendGain > 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        if (framesPlayed >= framesTotal)
        {
            stop();
            break;
        }

        lastEnv = envelopeAt (framesPlayed);

        float src = readMono (sourcePos);
        if (toneFilter.isActive())
            src = toneFilter.processSample (src);   // tone shapes the dry AND the send
        const float mono = src * lastEnv * levelGain;

        const int dest = startSample + n;

        // Post-envelope, pre-pan: the send is mono, and the reverb hears the voice at
        // the level you hear it at.
        if (feedsSend)
            sendOut[dest] += mono * sendGain;

        if (outChannels >= 2)
        {
            buffer.addSample (0, dest, mono * leftGain);
            buffer.addSample (1, dest, mono * rightGain);
            for (int ch = 2; ch < outChannels; ++ch)
                buffer.addSample (ch, dest, mono * monoGain);
        }
        else if (outChannels == 1)
        {
            buffer.addSample (0, dest, mono * monoGain);
        }

        sourcePos += increment;
        ++framesPlayed;
    }
}

} // namespace rollforge
