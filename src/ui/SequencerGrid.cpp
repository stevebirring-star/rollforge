#include "ui/SequencerGrid.h"

namespace rollforge
{

SequencerGrid::SequencerGrid (int lanes, int steps)
    : numLanes (juce::jmax (1, lanes)),
      numSteps (juce::jmax (1, steps))
{
    for (int lane = 0; lane < numLanes; ++lane)
    {
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

void SequencerGrid::setStep (int lane, int step, bool on, float velocity)
{
    if (auto* c = cell (lane, step))
        c->setState (on, velocity);
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
    const int gridW = area.getWidth() - labelWidth;
    const int cellW = gridW / numSteps;

    for (int lane = 0; lane < numLanes; ++lane)
    {
        const int y = lane * rowH;
        if (auto* label = laneLabels[lane])
            label->setBounds (0, y, labelWidth - 4, rowH);

        for (int step = 0; step < numSteps; ++step)
            if (auto* c = cell (lane, step))
                c->setBounds (labelWidth + step * cellW, y, cellW, rowH);
    }
}

} // namespace rollforge
