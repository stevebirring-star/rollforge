// RollForge test runner.
//
// Discovers every juce::UnitTest registered in the binary and runs it. Returns
// a non-zero exit code if any test fails so CTest / CI go red on regressions.

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

/** Phase 0 smoke test: proves the harness compiles, links and reports
    failures correctly. Replaced/augmented by real model tests in Phase 1+. */
class SmokeTest final : public juce::UnitTest
{
public:
    SmokeTest() : juce::UnitTest ("RollForge smoke", "rollforge") {}

    void runTest() override
    {
        beginTest ("arithmetic sanity");
        expectEquals (2 + 2, 4);

        beginTest ("jlimit clamps to range");
        expectEquals (juce::jlimit (0, 10, 15), 10);
        expectEquals (juce::jlimit (0, 10, -3), 0);
        expectEquals (juce::jlimit (0, 10, 7), 7);

        beginTest ("String round-trips through var");
        const juce::var v ("rollforge");
        expect (v.toString() == "rollforge");
    }
};

static SmokeTest smokeTest;

} // namespace rollforge::tests

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* result = runner.getResult (i))
            failures += result->failures;

    if (failures > 0)
    {
        juce::Logger::writeToLog ("RollForge tests FAILED: " + juce::String (failures) + " failure(s)");
        return 1;
    }

    juce::Logger::writeToLog ("RollForge tests passed.");
    return 0;
}
