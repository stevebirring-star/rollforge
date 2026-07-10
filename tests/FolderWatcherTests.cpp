// RollForge — FolderWatcher tests (P2: auto-ingest).
//
// These drive the real worker thread against a real temp folder — the point of the class is
// the handoff between two threads, and a stubbed one would test nothing. What they pin:
//
//   * a file dropped into a watched folder is ingested, without anyone pressing rescan;
//   * a file already in the library is never analysed twice, even while its row is still
//     sitting in the queue waiting to be written;
//   * the DB is only ever written by the thread that calls drainIntoDb();
//   * forgetting a folder does not forget its samples.

#include "library/FolderWatcher.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

namespace
{
    /** A short, non-degenerate mono WAV: silence analyses to nothing useful. */
    bool writeTestWav (const juce::File& file, int frames = 2048, int seedOffset = 0)
    {
        file.deleteFile();
        file.getParentDirectory().createDirectory();

        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));
        if (writer == nullptr)
            return false;

        stream.release();   // the writer owns it now

        juce::AudioBuffer<float> buffer (1, frames);
        juce::Random random (1234 + seedOffset);
        for (int i = 0; i < frames; ++i)
        {
            const float envelope = 1.0f - (float) i / (float) frames;
            buffer.setSample (0, i, (random.nextFloat() * 2.0f - 1.0f) * envelope * 0.7f);
        }

        return writer->writeFromAudioSampleBuffer (buffer, 0, frames);
    }

    /** A scratch folder that cleans itself up. */
    struct TempFolder
    {
        TempFolder()
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("RollForgeWatcherTest")
                       .getChildFile (juce::Uuid().toString()))
        {
            dir.createDirectory();
        }

        ~TempFolder() { dir.deleteRecursively(); }

        juce::File dir;
    };
}

class FolderWatcherTest final : public juce::UnitTest
{
public:
    FolderWatcherTest() : juce::UnitTest ("RollForge FolderWatcher", testCategory) {}

    void runTest() override
    {
        constexpr int timeoutMs = 15000;   // generous: a loaded CI box decodes slowly

        beginTest ("a watched folder is ingested in the background, and drained on demand");
        {
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));
            expect (writeTestWav (temp.dir.getChildFile ("hat_b.wav"), 1024, 7));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs), "the worker never finished a pass");

            // The worker analysed them; it did NOT write them. Nothing is in the DB yet.
            expectEquals (db.count(), 0, "the worker thread wrote to the database");
            expectEquals (watcher.pendingRows(), 2);

            expectEquals (watcher.drainIntoDb(), 2);
            expectEquals (db.count(), 2);
            expectEquals (watcher.totalAdded(), 2);
        }

        beginTest ("a file dropped in later is picked up without anyone asking");
        {
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.drainIntoDb(), 1);
            expectEquals (db.count(), 1);

            // This is the feature. No rescan, no button.
            expect (writeTestWav (temp.dir.getChildFile ("snare_c.wav"), 1500, 3));
            expect (watcher.waitForScanPass (timeoutMs));

            expectEquals (watcher.drainIntoDb(), 1, "the new file was not noticed");
            expectEquals (db.count(), 2);
        }

        beginTest ("nothing is analysed twice, not even while its row waits in the queue");
        {
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.pendingRows(), 1);

            // Let several more passes run WITHOUT draining. The row is analysed but not yet in
            // the DB, so a watcher that only checked the DB would queue it again, and again.
            expect (watcher.waitForScanPass (timeoutMs));
            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.pendingRows(), 1, "the same file was analysed more than once");

            expectEquals (watcher.drainIntoDb(), 1);

            // And once it IS in the DB, it stays out of the queue.
            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.drainIntoDb(), 0);
            expectEquals (db.count(), 1);
        }

        beginTest ("files the loader cannot read are not offered to it twice a second");
        {
            TempFolder temp;
            temp.dir.getChildFile ("notes.txt").replaceWithText ("not a sample");
            temp.dir.getChildFile ("cover.png").replaceWithText ("nor this");
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.pendingRows(), 1, "a non-audio file was analysed");
            expectEquals (watcher.drainIntoDb(), 1);
        }

        beginTest ("nested folders are searched, because that is how sample packs arrive");
        {
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("Pack").getChildFile ("Kicks")
                                          .getChildFile ("kick_a.wav")));
            expect (writeTestWav (temp.dir.getChildFile ("Pack").getChildFile ("Hats")
                                          .getChildFile ("hat_b.wav"), 900, 5));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.drainIntoDb(), 2);
        }

        beginTest ("the drain is batched, so a huge import cannot stall the message thread");
        {
            TempFolder temp;
            for (int i = 0; i < 5; ++i)
                expect (writeTestWav (temp.dir.getChildFile ("s" + juce::String (i) + ".wav"), 512, i));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);

            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.pendingRows(), 5);

            expectEquals (watcher.drainIntoDb (2), 2);
            expectEquals (watcher.drainIntoDb (2), 2);
            expectEquals (watcher.drainIntoDb (2), 1, "the last, short batch");
            expectEquals (watcher.drainIntoDb (2), 0);
            expectEquals (db.count(), 5);
        }

        beginTest ("watched folders persist, and forgetting one does not forget its samples");
        {
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));

            LibraryDb db;
            expect (db.openInMemory());

            {
                FolderWatcher watcher (db);
                watcher.setPollIntervalMs (25);
                watcher.start();
                watcher.addFolder (temp.dir);

                expect (watcher.waitForScanPass (timeoutMs));
                expectEquals (watcher.drainIntoDb(), 1);
            }

            expectEquals (db.watchedFolders().size(), 1, "the folder was not persisted");
            expectEquals (db.watchedFolders()[0], temp.dir.getFullPathName());

            // A second watcher over the same DB picks the folder back up, and finds nothing new.
            {
                FolderWatcher watcher (db);
                watcher.setPollIntervalMs (25);
                watcher.start();

                expectEquals (watcher.folders().size(), 1, "the folder was not reloaded");
                expect (watcher.waitForScanPass (timeoutMs));
                expectEquals (watcher.drainIntoDb(), 0, "an already-known file was re-analysed");

                watcher.removeFolder (temp.dir);
                expect (watcher.folders().isEmpty());
            }

            expect (db.watchedFolders().isEmpty(), "the folder is no longer watched");
            expectEquals (db.count(), 1, "its samples were thrown away with it");
        }

        beginTest ("a folder that has vanished is skipped, not treated as an empty one");
        {
            // The unmounted-drive case. Every row from that folder must survive.
            TempFolder temp;
            expect (writeTestWav (temp.dir.getChildFile ("kick_a.wav")));

            LibraryDb db;
            expect (db.openInMemory());

            FolderWatcher watcher (db);
            watcher.setPollIntervalMs (25);
            watcher.start();
            watcher.addFolder (temp.dir);
            expect (watcher.waitForScanPass (timeoutMs));
            expectEquals (watcher.drainIntoDb(), 1);

            watcher.addFolder (temp.dir.getChildFile ("does-not-exist"));   // silently ignored
            expectEquals (watcher.folders().size(), 1, "a non-directory was added to the watch list");

            temp.dir.deleteRecursively();
            expect (watcher.waitForScanPass (timeoutMs), "the worker died on a missing folder");
            expectEquals (db.count(), 1, "rows were deleted because the folder went away");
        }
    }
};

static FolderWatcherTest folderWatcherTest;

} // namespace rollforge::tests
