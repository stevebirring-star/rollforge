// RollForge test runner.
//
// Runs every juce::UnitTest registered under the RollForge category and returns
// a non-zero exit code if any fails, so CTest / CI go red on regressions.
//
// We deliberately run ONLY our own category — not juce::UnitTestRunner's
// runAllTests(), which would also execute JUCE's large internal unit-test
// suite. Those are JUCE's responsibility (tested upstream), they slow every CI
// run, and under UBSan some of them trip vptr diagnostics unrelated to our code.

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

// Every RollForge test registers under this category. Compile-time constant so
// there is no static-initialisation-order dependency with the test objects.
static constexpr const char* testCategory = "rollforge";

/** Phase 0 smoke test: proves the harness compiles, links and reports
    failures correctly. Replaced/augmented by real model tests in Phase 1+. */
class SmokeTest final : public juce::UnitTest
{
public:
    SmokeTest() : juce::UnitTest ("RollForge smoke", testCategory) {}

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
    runner.runTestsInCategory (rollforge::tests::testCategory);

    int failures = 0;
    int numTests = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult (i))
        {
            failures += result->failures;
            ++numTests;
        }
    }

    if (numTests == 0)
    {
        juce::Logger::writeToLog ("RollForge tests: NO tests ran for category '"
                                  + juce::String (rollforge::tests::testCategory) + "'");
        return 1;
    }

    if (failures > 0)
    {
        juce::Logger::writeToLog ("RollForge tests FAILED: " + juce::String (failures) + " failure(s)");
        return 1;
    }

    juce::Logger::writeToLog ("RollForge tests passed (" + juce::String (numTests) + " test group(s)).");
    return 0;
}
