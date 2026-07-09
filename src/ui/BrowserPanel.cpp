#include "ui/BrowserPanel.h"

#include "library/Scanner.h"
#include "library/Similarity.h"
#include "ui/RollForgeLookAndFeel.h"
#include "ui/Theme.h"

namespace rollforge
{

BrowserPanel::BrowserPanel (LibraryDb& dbToUse) : db (dbToUse)
{
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

    // Two readings of one library. The button names the view you'd switch TO.
    viewButton.onClick = [this] { setMapView (! map.isVisible()); };
    viewButton.setTooltip ("See the library laid out by sound: near dots are near sounds");
    addAndMakeVisible (viewButton);

    map.onAudition = [this] (int index)
    {
        if (onAudition != nullptr && index >= 0 && index < (int) allEntries.size())
            onAudition (allEntries[(size_t) index].path);
    };
    map.onContextMenu = [this] (int index)
    {
        if (index >= 0 && index < (int) allEntries.size())
            showMenuFor (allEntries[(size_t) index], &map);
    };
    addChildComponent (map);   // starts hidden; the list leads

    // NEW KIT makes something, so it wears the hot accent, like Make a Beat.
    newKitButton.setColour (juce::TextButton::buttonColourId, theme().accentHot);
    newKitButton.setColour (juce::TextButton::textColourOffId, theme().background);

    statusLabel.setColour (juce::Label::textColourId, theme().textDim);
    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
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

            if (onLibraryChanged != nullptr)
                onLibraryChanged();
        });
}

void BrowserPanel::refresh()
{
    if (! db.isOpen())
    {
        entries.clear();
        allEntries.clear();
        list.updateContent();
        list.repaint();
        map.setCorpus ({}, {});
        statusLabel.setText ("Library unavailable (could not open library.db)", juce::dontSendNotification);
        return;
    }

    // The map is always built from the WHOLE library, never from the filtered subset:
    // re-fitting the projection to a filter would scatter the survivors across the view and
    // read as the library rearranging itself. The filter only decides what is drawn solid.
    allEntries = db.all();

    const int sel = categoryFilter.getSelectedId();
    entries = (sel <= 1) ? allEntries
                         : db.byCategory ((SoundCategory) (sel - 2));

    list.updateContent();
    list.repaint();

    map.setCorpus (allEntries, Similarity::project (Similarity::build (allEntries)));
    map.setCategoryFilter (selectedCategory());

    statusLabel.setText (sel <= 1 ? juce::String (allEntries.size()) + " samples"
                                  : juce::String (entries.size()) + " of "
                                        + juce::String (allEntries.size()) + " samples",
                         juce::dontSendNotification);
}

std::optional<SoundCategory> BrowserPanel::selectedCategory() const
{
    const int sel = categoryFilter.getSelectedId();
    if (sel <= 1)
        return std::nullopt;
    return (SoundCategory) (sel - 2);
}

const LibraryEntry* BrowserPanel::filteredEntry (int row) const
{
    if (row < 0 || row >= (int) entries.size())
        return nullptr;
    return &entries[(size_t) row];
}

void BrowserPanel::setMapView (bool showMap)
{
    map.setVisible (showMap);
    list.setVisible (! showMap);
    viewButton.setButtonText (showMap ? "List" : "Map");
    repaint();
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

    const auto& t = theme();
    const auto& e = entries[(size_t) row];
    const auto  b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    // A quiet zebra so a long list stays scannable, then the selection over it.
    if (row % 2 == 1)
    {
        g.setColour (t.panelRaised.withAlpha (0.35f));
        g.fillRect (b);
    }
    if (selected)
    {
        g.setColour (t.accentCool.withAlpha (0.22f));
        g.fillRoundedRectangle (b.reduced (2.0f, 1.0f), 3.0f);
        g.setColour (t.accentCool.withAlpha (0.55f));
        g.drawRoundedRectangle (b.reduced (2.0f, 1.0f), 3.0f, 1.0f);
    }

    // The category's colour, here and on the pad it will land on. Same sound, same hue.
    const auto colour = t.colourFor (e.category);
    g.setColour (colour);
    g.fillEllipse (10.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);

    g.setColour (t.text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (e.name, 24, 0, width - 116, height, juce::Justification::centredLeft, true);

    g.setColour (colour.withMultipliedSaturation (0.6f).withMultipliedBrightness (0.95f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (categoryName (e.category), width - 96, 0, 86, height, juce::Justification::centredRight, true);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.fillAll (t.background);

    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (28 + 6);

    // The list sits in a well, the way the sequencer does. The map draws its own.
    if (! map.isVisible())
        RollForgeLookAndFeel::drawRecessedWell (g, r.withTrimmedBottom (24).toFloat(), 5.0f);

    g.setColour (t.hairline);
    g.drawLine ((float) r.getX(), (float) r.getY() - 4.0f, (float) r.getRight(), (float) r.getY() - 4.0f, 1.0f);
}

void BrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const auto* entry = filteredEntry (row);
    if (entry == nullptr)
        return;

    if (e.mods.isPopupMenu())
    {
        showMenuFor (*entry, &list);
        return;
    }

    // A plain click auditions the sample through the engine's preview pad, so you can
    // hear what you're browsing without disturbing the kit.
    if (onAudition != nullptr)
        onAudition (entry->path);
}

void BrowserPanel::showMenuFor (const LibraryEntry& entry, juce::Component* target)
{
    const juce::String  path    = entry.path;
    const juce::String  name    = entry.name;
    const SoundCategory current = entry.category;

    // Menu ids: 1..16 = send to that pad; 100 = slice; 200+ = re-tag as that category.
    constexpr int sendBase  = 1;
    constexpr int sliceId   = 100;
    constexpr int retagBase = 200;

    juce::PopupMenu sendMenu;
    for (int p = 0; p < kitNumPads; ++p)
        sendMenu.addItem (sendBase + p, "Pad " + juce::String (p + 1));

    // Re-tagging corrects the auto-tagger, which gets things wrong sometimes and which
    // Atlas flatly can't fix. The override is persisted and survives a re-scan.
    juce::PopupMenu retagMenu;
    for (int i = 0; i <= (int) SoundCategory::Fx; ++i)          // real categories, not Unknown
        retagMenu.addItem (retagBase + i, categoryName ((SoundCategory) i),
                           /*enabled*/ true, /*ticked*/ current == (SoundCategory) i);

    juce::PopupMenu menu;
    menu.addSectionHeader (name.substring (0, 28));
    menu.addSubMenu ("Send to pad", sendMenu);
    menu.addItem (sliceId, "Slice across the pads");
    menu.addSeparator();
    menu.addSubMenu ("Re-tag as", retagMenu);

    juce::Component::SafePointer<BrowserPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
        [safe, path] (int choice)
        {
            if (safe == nullptr || choice <= 0)
                return;

            if (choice >= retagBase)
            {
                safe->db.setCategoryOverride (path, (SoundCategory) (choice - retagBase));
                safe->refresh();

                // A re-tag moves the sample into a different category, and "Similar" is
                // gated on category — so the owner's search must be told.
                if (safe->onLibraryChanged != nullptr)
                    safe->onLibraryChanged();
            }
            else if (choice == sliceId)
            {
                if (safe->onSliceLoop != nullptr)
                    safe->onSliceLoop (path);
            }
            else if (safe->onSendToPad != nullptr)
            {
                safe->onSendToPad (path, choice - sendBase);
            }
        });
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (28);
    scanButton.setBounds (top.removeFromLeft (110));
    top.removeFromLeft (6);
    newKitButton.setBounds (top.removeFromLeft (90));
    top.removeFromLeft (6);
    viewButton.setBounds (top.removeFromRight (64));
    top.removeFromRight (6);
    categoryFilter.setBounds (top.removeFromLeft (juce::jmin (130, top.getWidth())));

    r.removeFromTop (6);
    statusLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    list.setBounds (r.reduced (3));   // sits inside the well painted in paint()
    map.setBounds (r);                // the map paints its own well, edge to edge
}

} // namespace rollforge
