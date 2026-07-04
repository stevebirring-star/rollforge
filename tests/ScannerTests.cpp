// RollForge — Scanner tests (Phase 5, commit 3).
//
// Headless: writing a couple of named WAVs into a temp folder, scanning it
// decodes + categorises + stores them in the DB.

#include "library/LibraryDb.h"
#include "library/Scanner.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <memory>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    bool writeWav (const juce::File& file, double sr)
    {
        file.deleteFile();
        juce::WavAudioFormat fmt;
        auto os = file.createOutputStream();
        if (os == nullptr)
            return false;

        std::unique_ptr<juce::AudioFormatWriter> writer (
            fmt.createWriterFor (os.get(), sr, 1u, 16, juce::StringPairArray(), 0));
        if (writer == nullptr)
            return false;
        os.release();   // the writer owns the stream now

        const int n = 2205;   // ~50 ms
        juce::AudioBuffer<float> buf (1, n);
        for (int i = 0; i < n; ++i)
            buf.setSample (0, i, 0.3f * std::sin (2.0f * 3.14159265f * 220.0f * (float) i / (float) sr)
                                      * (1.0f - (float) i / (float) n));
        return writer->writeFromAudioSampleBuffer (buf, 0, n);
        // writer's destructor flushes + closes the file.
    }
}

class ScannerTest final : public juce::UnitTest
{
public:
    ScannerTest() : juce::UnitTest ("RollForge Scanner", testCategory) {}

    void runTest() override
    {
        beginTest ("scans a folder + populates the DB with categories");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("rollforge_scan_test");
            dir.deleteRecursively();
            dir.createDirectory();

            const double sr = 44100.0;
            expect (writeWav (dir.getChildFile ("Kick_low.wav"), sr));
            expect (writeWav (dir.getChildFile ("HiHat_bright.wav"), sr));

            LibraryDb db;
            expect (db.openInMemory());
            Scanner scanner (db);

            const int stored = scanner.scanBlocking (dir);
            expectEquals (stored, 2);
            expectEquals (db.count(), 2);
            expectEquals (scanner.getScannedCount(), 2);

            const auto kicks = db.byCategory (SoundCategory::Kick);   // by filename token
            expectEquals ((int) kicks.size(), 1);
            expect (kicks[0].name == juce::String ("Kick_low"));
            expect (kicks[0].durationSeconds > 0.0f);

            dir.deleteRecursively();
        }
    }
};

static ScannerTest scannerTest;

} // namespace rollforge::tests
