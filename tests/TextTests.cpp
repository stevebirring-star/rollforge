// RollForge — Text tests.
//
// One test, and it exists to pin down a trap rather than to check arithmetic: juce::String
// built from a plain `const char*` decodes it as LATIN-1. Every UI literal with an em-dash
// or a bullet in it was silently rendering as mojibake because of this.

#include "ui/Text.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class TextTest final : public juce::UnitTest
{
public:
    TextTest() : juce::UnitTest ("RollForge Text", testCategory) {}

    void runTest() override
    {
        beginTest ("utf8() decodes a source literal as UTF-8, which juce::String does not");
        {
            // Three bytes: e2 80 94. As UTF-8 that is one character (an em-dash); read as
            // Latin-1 -- which is what juce::String(const char*) does -- it is three.
            const char* emDash = "\xe2\x80\x94";

            expectEquals (utf8 (emDash).length(), 1, "utf8() must yield one character");
            expectEquals (utf8 (emDash)[0], (juce::juce_wchar) 0x2014, "and it must be the em-dash");

            // The trap itself. If a future JUCE ever fixes this, the assertion below fails and
            // whoever sees it can retire utf8() on purpose rather than by accident.
            expectEquals (juce::String (juce::CharPointer_ASCII (emDash)).length(), 3,
                          "juce::String(const char*) no longer reads bytes as Latin-1");
        }

        beginTest ("plain ASCII is untouched, so utf8() is safe to wrap anything in");
        {
            expectEquals (utf8 ("Evolve off - keeping the variation"),
                          juce::String ("Evolve off - keeping the variation"));
            expect (utf8 ("").isEmpty());
        }
    }
};

static TextTest textTest;

} // namespace rollforge::tests
