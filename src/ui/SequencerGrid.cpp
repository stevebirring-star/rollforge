#include "ui/SequencerGrid.h"

#include "ui/GridGeometry.h"

namespace rollforge
{

SequencerGrid::SequencerGrid (int lanes, int steps)
    : numLanes (juce::jmax (1, lanes)),
      numSteps (juce::jmax (1, steps))
{
    lanePlayhead.assign ((std::size_t) numLanes, -1);
    laneLength.assign ((std::size_t) numLanes, numSteps);

    for (int lane = 0; lane < numLanes; ++lane)
    {
        auto* lock = lockButtons.add (new LaneLockButton());
        lock->onToggle = [this, lane]
        {
            if (onLaneLockToggled)
                onLaneLockToggled (lane);
        };
        addAndMakeVisible (lock);

        auto* trip = tripletButtons.add (new juce::TextButton ("3"));
        trip->setClickingTogglesState (true);
        trip->setWantsKeyboardFocus (false);
        trip->setTooltip ("Run this lane in 1/8-note triplets (12 steps to the bar) "
                          "instead of straight 1/16ths");
        trip->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff23232a));
        trip->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0a341));
        trip->setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff70707c));
        trip->setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        trip->onClick = [this, lane]
        {
            if (onLaneTripletToggled)
                onLaneTripletToggled (lane, tripletButtons[lane]->getToggleState());
        };
        addAndMakeVisible (trip);

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

void SequencerGrid::setLaneLength (int lane, int length)
{
    if (lane < 0 || lane >= numLanes)
        return;

    laneLength[(std::size_t) lane] = juce::jlimit (1, numSteps, length);
    for (int step = 0; step < numSteps; ++step)
        if (auto* c = cell (lane, step))
            c->setActive (step < laneLength[(std::size_t) lane]);
}

void SequencerGrid::setLaneTriplet (int lane, bool triplet)
{
    if (auto* b = tripletButtons[lane])
        b->setToggleState (triplet, juce::dontSendNotification);
}

void SequencerGrid::setLaneColour (int lane, juce::Colour colour)
{
    if (lane < 0 || lane >= numLanes)
        return;

    if (auto* label = laneLabels[lane])
        label->setColour (juce::Label::textColourId, colour.withMultipliedSaturation (0.75f)
                                                           .withMultipliedBrightness (0.95f));
    for (int step = 0; step < numSteps; ++step)
        if (auto* c = cell (lane, step))
            c->setAccent (colour);
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

void SequencerGrid::setLanePlayhead (int lane, int step)
{
    if (lane < 0 || lane >= numLanes)
        return;

    int& current = lanePlayhead[(std::size_t) lane];
    if (step == current)
        return;

    if (auto* prev = cell (lane, current)) prev->setPlayhead (false);
    if (auto* now  = cell (lane, step))    now->setPlayhead (true);
    current = step;
}

void SequencerGrid::clearPlayheads()
{
    for (int lane = 0; lane < numLanes; ++lane)
        setLanePlayhead (lane, -1);
}

void SequencerGrid::resized()
{
    auto area = getLocalBounds();
    const int gridW = juce::jmax (1, area.getWidth() - labelColumnWidth);

    constexpr int lockW = 18;   // narrow padlock column at the left of each lane header
    constexpr int tripW = 18;   // and the "3" triplet toggle beside it
    for (int lane = 0; lane < numLanes; ++lane)
    {
        // Exact edges, never a truncated width: see ui/GridGeometry.h. The lanes and the
        // steps both tile their space, so nothing drifts as the window resizes.
        const auto rowY = gridSpan (lane, numLanes, area.getHeight());
        const int  y    = rowY.getStart();
        const int  rowH = rowY.getLength();

        if (auto* lock = lockButtons[lane])
            lock->setBounds (0, y, lockW, rowH);
        if (auto* trip = tripletButtons[lane])
            trip->setBounds (lockW, y + 2, tripW, juce::jmax (10, rowH - 4));
        if (auto* label = laneLabels[lane])
            label->setBounds (lockW + tripW + 2, y, labelColumnWidth - lockW - tripW - 6, rowH);

        for (int step = 0; step < numSteps; ++step)
            if (auto* c = cell (lane, step))
            {
                const auto sx = gridSpan (step, numSteps, gridW, labelColumnWidth);
                c->setBounds (sx.getStart(), y, sx.getLength(), rowH);
            }
    }
}

} // namespace rollforge
