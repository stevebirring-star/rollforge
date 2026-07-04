#include "ui/BrowserPanel.h"

#include "library/Scanner.h"

namespace rollforge
{

BrowserPanel::BrowserPanel()
{
    auto dbFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("RollForge")
                      .getChildFile ("library.db");
    dbFile.getParentDirectory().createDirectory();
    db.open (dbFile);

    scanButton.onClick = [this] { chooseFolderAndScan(); };
    addAndMakeVisible (scanButton);

    newKitButton.onClick = [this] { rebuildKit(); };
    addAndMakeVisible (newKitButton);

    categoryFilter.addItem ("All", 1);
    for (int i = 0; i <= (int) SoundCategory::Fx; ++i)
        categoryFilter.addItem (categoryName ((SoundCategory) i), i + 2);
    categoryFilter.setSelectedId (1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { refresh(); };
    addAndMakeVisible (categoryFilter);

    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa4));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    list.setModel (this);
    list.setRowHeight (22);
    addAndMakeVisible (list);

    refresh();
    setSize (460, 420);
}

BrowserPanel::~BrowserPanel()
{
    list.setModel (nullptr);
}

void BrowserPanel::chooseFolderAndScan()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a samples folder");
    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();
            if (! folder.isDirectory())
                return;

            statusLabel.setText ("Scanning...", juce::dontSendNotification);
            repaint();

            Scanner scanner (db);
            const int n = scanner.scanBlocking (folder);   // synchronous for now
            statusLabel.setText (juce::String (n) + " samples added", juce::dontSendNotification);
            refresh();
        });
}

void BrowserPanel::refresh()
{
    const int sel = categoryFilter.getSelectedId();
    entries = (sel <= 1) ? db.all()
                         : db.byCategory ((SoundCategory) (sel - 2));
    list.updateContent();
    list.repaint();

    if (sel <= 1)
        statusLabel.setText (juce::String (db.count()) + " samples", juce::dontSendNotification);
}

void BrowserPanel::rebuildKit()
{
    KitBuilder kb (db);
    std::array<bool, kitNumPads> unlocked {};
    const auto selection = kb.build (KitBuilder::Selection {}, kitSeed++, unlocked);
    if (onNewKit != nullptr)
        onNewKit (selection.paths);
}

int BrowserPanel::getNumRows()
{
    return (int) entries.size();
}

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) entries.size())
        return;

    if (selected)
        g.fillAll (juce::Colour (0xff2a7a74));

    const auto& e = entries[(size_t) row];
    g.setColour (juce::Colour (0xffe8e8ec));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (e.name, 8, 0, width - 96, height, juce::Justification::centredLeft, true);

    g.setColour (juce::Colour (0xff9a9aa4));
    g.drawText (categoryName (e.category), width - 90, 0, 84, height, juce::Justification::centredRight, true);
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    scanButton.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (6);
    newKitButton.setBounds (top.removeFromLeft (90));
    top.removeFromLeft (6);
    categoryFilter.setBounds (top.removeFromLeft (130));

    r.removeFromTop (6);
    statusLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    list.setBounds (r);
}

} // namespace rollforge
