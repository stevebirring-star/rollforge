#include "ui/SequencerGrid.h"

namespace rollforge
{

SequencerGrid::SequencerGrid (int lanes, int steps)
    : numLanes (juce::jmax (1, lanes)),
      numSteps (juce::jmax (1, steps))
{
    for (int lane = 0; lane < numLanes; ++lane)
    {
        auto* lock = lockButtons.add (new LaneLockButton());
        lock->onToggle = [this, lane]
        {
            if (onLaneLockToggled)
                onLaneLockToggled (lane);
        };
        addAndMakeVisible (lock);

        auto* label = laneLabels.add (new juce::Label());
        label->setFont (juce::FontOptions (12.0f));
        label->setColour (juce::Label::textColourId, juce::Colour (0xffbcbcc4));
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);

        for (int step = 0; step < numSteps; ++step)
        {
            auto* c = cells.add (new StepComponent());
            c->onEdit = [this, lane, step] (bool on, float velocity)
            {
                if (onStepEdit)
                    onStepEdit (lane, step, on, velocity);
            };
            c->onGestureStart = [this]
            {
                if (onGestureStart)
                    onGestureStart();
            };
            addAndMakeVisible (c);
        }
    }
}

StepComponent* SequencerGrid::cell (int lane, int step) noexcept
{
    if (lane < 0 || lane >= numLanes || step < 0 || step >= numSteps)
        return nullptr;
    return cells[lane * numSteps + step];
}

void SequencerGrid::setLaneLabel (int lane, const juce::String& text)
{
    if (auto* label = laneLabels[lane])
        label->setText (text, juce::dontSendNotification);
}

void SequencerGrid::setLaneLocked (int lane, bool locked)
{
    if (auto* b = lockButtons[lane])
        b->setLocked (locked);
}

void SequencerGrid::setStep (int lane, int step, bool on, float velocity)
{
    if (auto* c = cell (lane, step))
        c->setState (on, velocity);
}

void SequencerGrid::flashChanged (const std::vector<std::pair<int, int>>& changedCells)
{
    // Clear any previous flash first so a rapid re-vary doesn't leave stale rings.
    for (const auto& [lane, step] : flashed)
        if (auto* c = cell (lane, step))
            c->setChanged (false);
    flashed.clear();

    for (const auto& [lane, step] : changedCells)
        if (auto* c = cell (lane, step))
        {
            c->setChanged (true);
            flashed.push_back ({ lane, step });
        }

    if (! flashed.empty())
        startTimer (1100);   // one-shot: cleared in timerCallback
    else
        stopTimer();
}

void SequencerGrid::timerCallback()
{
    for (const auto& [lane, step] : flashed)
        if (auto* c = cell (lane, step))
            c->setChanged (false);
    flashed.clear();
    stopTimer();
}

void SequencerGrid::setPlayheadStep (int step)
{
    if (step == playheadStep)
        return;

    for (int lane = 0; lane < numLanes; ++lane)
    {
        if (auto* prev = cell (lane, playheadStep)) prev->setPlayhead (false);
        if (auto* now  = cell (lane, step))         now->setPlayhead (true);
    }
    playheadStep = step;
}

void SequencerGrid::resized()
{
    auto area = getLocalBounds();
    const int rowH = area.getHeight() / numLanes;
    const int gridW = area.getWidth() - labelColumnWidth;
    const int cellW = gridW / numSteps;

    constexpr int lockW = 18;   // narrow padlock column at the left of each lane header
    for (int lane = 0; lane < numLanes; ++lane)
    {
        const int y = lane * rowH;
        if (auto* lock = lockButtons[lane])
            lock->setBounds (0, y, lockW, rowH);
        if (auto* label = laneLabels[lane])
            label->setBounds (lockW, y, labelColumnWidth - lockW - 4, rowH);

        for (int step = 0; step < numSteps; ++step)
            if (auto* c = cell (lane, step))
                c->setBounds (labelColumnWidth + step * cellW, y, cellW, rowH);
    }
}

} // namespace rollforge
