#include "ui/PatternSlots.h"

#include "ui/GridGeometry.h"
#include "ui/Theme.h"

namespace rollforge
{

//==============================================================================
/** One lettered key. It draws itself rather than wearing a TextButton's clothes, because
    four states (current / queued / written / empty) do not map onto a toggle. */
class PatternSlots::SlotButton final : public juce::Component,
                                       public juce::SettableTooltipClient
{
public:
    SlotButton (int slotIndex, PatternSlots& ownerToUse) : owner (ownerToUse), slot (slotIndex)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            owner.showSlotMenu (slot);
        else if (owner.onSelect != nullptr)
            owner.onSelect (slot);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t         = theme();
        const auto  b         = getLocalBounds().toFloat().reduced (1.0f);
        const bool  isCurrent = (slot == owner.current);
        const bool  isQueued  = (slot == owner.queued);

        auto fill = t.panelRaised;
        if (isCurrent)
            fill = t.accentHot;
        else if (hovered)
            fill = t.panelRaised.brighter (0.25f);

        g.setColour (fill);
        g.fillRoundedRectangle (b, 3.0f);

        // The queued slot breathes in ember — a VALUE changing, not an action being taken.
        if (isQueued)
        {
            g.setColour (t.ember.withAlpha (0.35f + 0.55f * owner.pulse));
            g.drawRoundedRectangle (b.reduced (0.75f), 3.0f, 1.6f);
        }
        else
        {
            g.setColour (t.hairline);
            g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        }

        // Written vs empty: the letter carries it. A full-strength glyph means "there is
        // something here"; a ghost means "nothing has ever played out of this one".
        auto textColour = isCurrent ? t.background
                                    : (written ? t.text : t.textFaint);
        g.setColour (textColour);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + slot)),
                    getLocalBounds(), juce::Justification::centred);
    }

    void setWritten (bool w) { if (written != w) { written = w; repaint(); } }

private:
    PatternSlots& owner;
    const int     slot;
    bool          written = false;
    bool          hovered = false;
};

//==============================================================================
PatternSlots::PatternSlots()
{
    caption.setText ("PATTERN", juce::dontSendNotification);
    caption.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    caption.setColour (juce::Label::textColourId, theme().textDim);
    caption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (caption);

    for (int i = 0; i < numPatternSlots; ++i)
    {
        buttons[(std::size_t) i] = std::make_unique<SlotButton> (i, *this);
        buttons[(std::size_t) i]->setTooltip (
            "Pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + i))
                + " — click to switch (on the next bar while playing). "
                  "Right-click to copy the current pattern here, or clear it.");
        addAndMakeVisible (*buttons[(std::size_t) i]);
    }
}

PatternSlots::~PatternSlots() = default;

void PatternSlots::setCurrent (int slot)
{
    if (! PatternBank::isValidSlot (slot) || current == slot)
        return;
    current = slot;
    repaint();
    for (auto& b : buttons)
        b->repaint();
}

void PatternSlots::setQueued (int slot)
{
    if (queued == slot)
        return;

    queued = slot;
    phase  = 0.0f;
    pulse  = 0.0f;

    if (queued >= 0)
        startTimerHz (30);
    else
        stopTimer();

    for (auto& b : buttons)
        b->repaint();
}

void PatternSlots::setSlotWritten (int slot, bool written)
{
    if (PatternBank::isValidSlot (slot))
        buttons[(std::size_t) slot]->setWritten (written);
}

void PatternSlots::timerCallback()
{
    // A full breath every ~1.6 s: slow enough to read as "waiting", not as an alarm. `pulse`
    // is a triangle of the phase and so never leaves 0..1 — it is used directly as an alpha.
    phase += 1.0f / 24.0f;
    if (phase >= 2.0f)
        phase -= 2.0f;
    pulse = phase <= 1.0f ? phase : 2.0f - phase;

    if (PatternBank::isValidSlot (queued))
        buttons[(std::size_t) queued]->repaint();
}

void PatternSlots::showSlotMenu (int slot)
{
    constexpr int copyId  = 1;
    constexpr int clearId = 2;

    const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + slot));

    juce::PopupMenu menu;
    menu.addSectionHeader ("Pattern " + letter);
    menu.addItem (copyId, "Copy the current pattern here", slot != current);
    menu.addItem (clearId, "Clear");

    juce::Component::SafePointer<PatternSlots> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (buttons[(std::size_t) slot].get()),
        [safe, slot] (int choice)
        {
            if (safe == nullptr)
                return;
            if (choice == copyId && safe->onCopyTo != nullptr)
                safe->onCopyTo (slot);
            else if (choice == clearId && safe->onClear != nullptr)
                safe->onClear (slot);
        });
}

void PatternSlots::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromLeft (58));
    r.removeFromLeft (4);

    // Exact tiling: the eight keys share the row without a rounding gap opening up at H.
    const int total = juce::jmin (r.getWidth(), 34 * numPatternSlots);
    r = r.removeFromLeft (total);

    for (int i = 0; i < numPatternSlots; ++i)
    {
        const auto span = gridSpan (i, numPatternSlots, total, r.getX());
        buttons[(std::size_t) i]->setBounds (span.getStart(), r.getY(),
                                             span.getLength() - 3, r.getHeight());
    }
}

} // namespace rollforge
