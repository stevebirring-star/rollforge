#include "ui/DragExportButton.h"

namespace rollforge
{

namespace
{
    const juce::Colour chipFill     { 0xff2a3340 };
    const juce::Colour chipHover    { 0xff36435a };
    const juce::Colour chipOutline  { 0xff4a90d9 };
    const juce::Colour chipText     { 0xffe8e8ec };
}

DragExportButton::DragExportButton (juce::String labelText, juce::String tooltipText)
    : label (std::move (labelText))
{
    setTooltip (tooltipText);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::File DragExportButton::dragTempDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RollForge");
    dir.createDirectory();
    return dir;
}

void DragExportButton::sweepStaleTempFiles()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RollForge");
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false))
            f.deleteFile();
}

void DragExportButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (hovered ? chipHover : chipFill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (chipOutline.withAlpha (hovered ? 0.9f : 0.5f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // A grip: three short bars, the universal "pick me up" affordance.
    auto grip = r.removeFromLeft (18.0f).reduced (5.0f, 7.0f);
    g.setColour (chipOutline.withAlpha (0.8f));
    for (int i = 0; i < 3; ++i)
        g.fillRect (grip.getX(), grip.getY() + (float) i * grip.getHeight() / 2.5f, grip.getWidth(), 1.5f);

    g.setColour (chipText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (label, r, juce::Justification::centred, true);
}

void DragExportButton::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void DragExportButton::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void DragExportButton::mouseDown (const juce::MouseEvent&)
{
    // Render up front. On X11 externalDragFileInit() grabs the pointer synchronously,
    // and the receiving app reads the path from disk — possibly only after XdndFinished
    // — so the file has to be complete before the drag begins.
    dragStarted = false;
    pendingFile = (renderFile != nullptr) ? renderFile() : juce::File();
}

void DragExportButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || pendingFile == juce::File() || ! pendingFile.existsAsFile())
        return;
    if (e.getDistanceFromDragStart() < dragThresholdPixels)
        return;   // a click is not a drag

    dragStarted = true;

    // canMoveFiles = false: we hand out a temp copy, and a receiver that "moved" it
    // would delete ours. Deliberately no completion callback deleting the file —
    // sweepStaleTempFiles() reclaims it at the next launch instead.
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        { pendingFile.getFullPathName() }, /*canMoveFiles*/ false, /*sourceComponent*/ this);
}

void DragExportButton::mouseUp (const juce::MouseEvent&)
{
    dragStarted = false;
}

} // namespace rollforge
