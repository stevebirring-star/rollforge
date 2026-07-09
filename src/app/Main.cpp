#include "ui/MainComponent.h"
#include "ui/RollForgeLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class RollForgeApplication final : public juce::JUCEApplication
{
public:
    RollForgeApplication() = default;

    const juce::String getApplicationName() override    { return "RollForge"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        // The LookAndFeel must outlive every component that points at it, and it must be
        // torn down BEFORE JUCE's own default is destroyed — hence set/clear here rather
        // than in a component. This ordering is the classic dangling-LookAndFeel crash.
        lookAndFeel = std::make_unique<RollForgeLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    //==============================================================================
    /** Holds the UI at its design size and scrolls if the window is made smaller.

        setResizeLimits() cannot be relied on here. JUCE only adds the windowIsResizable
        style flag when Desktop::supportsBorderlessNonClientResize() is true, which X11
        reports as false — so LinuxComponentPeer::isConstrainedNativeWindow() is false, the
        peer never calls updateConstraints(), and no PMinSize ever reaches WM_NORMAL_HINTS.
        A window manager will therefore let the user drag the window to any size at all,
        and the layout — ~420 px of fixed rows plus two grids — piles up on itself.

        Rather than fight the window manager, the content keeps its minimum and the
        viewport scrolls. The limits are still set for Windows and macOS, where the flag is
        present and the WM does honour them. */
    class RootView final : public juce::Viewport
    {
    public:
        static constexpr int minWidth  = 780;
        static constexpr int minHeight = 874;   // + the A..H pattern row

        RootView()
        {
            setScrollBarsShown (true, true);
            setViewedComponent (new MainComponent(), true);
            setSize (minWidth, 880);
        }

        void resized() override
        {
            Viewport::resized();

            if (auto* content = getViewedComponent())
                content->setSize (juce::jmax (getMaximumVisibleWidth(),  minWidth),
                                  juce::jmax (getMaximumVisibleHeight(), minHeight));
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RootView)
    };

    /** Top-level desktop window hosting the MainComponent. */
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              theme().background,
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new RootView(), true);
            setResizable (true, false);

            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            // Honoured on Windows and macOS. On X11 it is silently a no-op (see RootView),
            // where the scrolling viewport is what actually protects the layout.
            setResizeLimits (RootView::minWidth, RootView::minHeight, 4000, 3000);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    // Declared AFTER the window in destruction order terms: `shutdown()` clears both
    // explicitly, but keep the LookAndFeel last so it outlives any stray component.
    std::unique_ptr<RollForgeLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow>           mainWindow;
};

} // namespace rollforge

START_JUCE_APPLICATION (rollforge::RollForgeApplication)
