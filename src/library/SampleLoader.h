#pragma once

// RollForge — SampleLoader: decodes audio files / streams into SampleBuffers.
//
// Decodes WAV / AIFF / FLAC / Ogg Vorbis (and MP3 if JUCE_USE_MP3AUDIOFORMAT is
// enabled) via juce::AudioFormatManager into a SampleBuffer at the file's NATIVE
// sample rate. Per the Phase-1 decision (HANDOFF §4) we do NOT resample on load:
// the Voice resamples per-voice to the device rate, which is robust to runtime
// device-rate changes. Sources with more than two channels are folded to their
// first two (a drum sampler is mono/stereo).
//
// THREADING: call load*() from a NON-AUDIO thread (message thread or a background
// loader / ThreadPool) — decoding allocates and does IO. The resulting
// SampleBuffer is then handed to the audio thread only via the command FIFO +
// retirement set (SampleBuffer.h), never directly.
//
// LIBRARY LAYER: no JUCE GUI includes.

#include "engine/SampleBuffer.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace rollforge
{

class SampleLoader
{
public:
    SampleLoader();

    /** Decodes an entire audio file into a SampleBuffer at its native rate.
        Returns null if the file is missing, unreadable, or an unsupported format. */
    SampleBuffer::Ptr loadFile (const juce::File& file);

    /** Decodes from an input stream (takes ownership). Returns null on failure. */
    SampleBuffer::Ptr loadFromStream (std::unique_ptr<juce::InputStream> stream, const juce::String& name);

    /** Decodes from an in-memory encoded block (copies the data). */
    SampleBuffer::Ptr loadFromMemory (const void* data, size_t numBytes, const juce::String& name);

    /** Space/`;`-separated wildcard list of the formats this loader recognises. */
    juce::String getSupportedWildcards() const { return formatManager.getWildcardForAllFormats(); }

private:
    SampleBuffer::Ptr readAll (juce::AudioFormatReader* reader, const juce::String& name);

    juce::AudioFormatManager formatManager;
};

} // namespace rollforge
