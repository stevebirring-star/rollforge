#pragma once

// RollForge — Text: build a juce::String from a UTF-8 source literal.
//
// juce::String (const char*) reads its bytes as LATIN-1, not UTF-8. From JUCE's own comment
// at juce_String.cpp:307: "If you get an assertion here, then you're trying to create a
// string from 8-bit data that contains values greater than 127. These can NOT be correctly
// converted to unicode..."
//
// So a literal with an em-dash or a bullet in it renders as mojibake in a Release build
// ("Evolve off â€” keeping...") and trips an assertion in Debug. It is a trap that costs
// nothing to fall into: the code looks right, and every ASCII string around it works.
//
// Every non-ASCII UI literal goes through utf8(). If you are writing a new one and it does
// not, it is wrong — grep for it.
//
// juce_core only; no GUI.

#include <juce_core/juce_core.h>

namespace rollforge
{

/** `literal` must be UTF-8 (which is what this repo's source files are). */
inline juce::String utf8 (const char* literal)
{
    return juce::String (juce::CharPointer_UTF8 (literal));
}

} // namespace rollforge
