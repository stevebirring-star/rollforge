#include "library/SampleLoader.h"

namespace rollforge
{

SampleLoader::SampleLoader()
{
    formatManager.registerBasicFormats();   // WAV, AIFF, FLAC, Ogg Vorbis (per module flags)

   #if JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat (new juce::MP3AudioFormat(), false);
   #endif
}

SampleBuffer::Ptr SampleLoader::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    // The name is a DISPLAY name: it goes on the pad face and the sequencer lane label.
    // Both already dropped the extension when they built it from the File themselves, and a
    // lane reading "Kick_808.wav" next to a pad reading "Kick_808" looked like two sounds.
    return readAll (reader.get(), file.getFileNameWithoutExtension());
}

SampleBuffer::Ptr SampleLoader::loadFromStream (std::unique_ptr<juce::InputStream> stream, const juce::String& name)
{
    if (stream == nullptr)
        return nullptr;

    // createReaderFor takes ownership of the stream (and deletes it on failure).
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (stream)));
    return readAll (reader.get(), name);
}

SampleBuffer::Ptr SampleLoader::loadFromMemory (const void* data, size_t numBytes, const juce::String& name)
{
    if (data == nullptr || numBytes == 0)
        return nullptr;

    auto stream = std::make_unique<juce::MemoryInputStream> (data, numBytes, /*keepInternalCopy*/ true);
    return loadFromStream (std::move (stream), name);
}

SampleBuffer::Ptr SampleLoader::readAll (juce::AudioFormatReader* reader, const juce::String& name)
{
    if (reader == nullptr)
        return nullptr;

    const int numSamples = (int) reader->lengthInSamples;
    if (numSamples <= 0)
        return nullptr;

    // Fold to mono/stereo (drum sampler); the float read overload fills L/R.
    const int numChannels = juce::jlimit (1, 2, (int) reader->numChannels);

    juce::AudioBuffer<float> audio (numChannels, numSamples);
    audio.clear();
    if (! reader->read (&audio, 0, numSamples, 0, true, numChannels > 1))
        return nullptr;

    const double rate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    return new SampleBuffer (std::move (audio), rate, name);
}

} // namespace rollforge
