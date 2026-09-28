#include "MidiClipView.h"

#include <algorithm>

namespace c2paseq
{
MidiClipView::MidiClipView(ArrangementMidiClipSnapshot snapshot,
                           int trackIndex,
                           juce::Colour colour)
    : clip(std::move(snapshot)), trackNumber(trackIndex), clipColour(colour)
{
}

void MidiClipView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(clipColour.withAlpha(0.9f));
    g.fillRect(bounds);
    g.setColour(selected ? juce::Colour::fromRGB(255, 213, 92)
                         : clipColour.brighter(0.4f));
    g.drawRect(bounds.reduced(0.5f), selected ? 2.0f : 1.0f);

    g.setColour(juce::Colour::fromRGB(232, 238, 232));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawFittedText("MIDI clip  •  " + juce::String(static_cast<int>(clip.notes.size()))
                         + " notes",
                     getLocalBounds().reduced(8).removeFromTop(18),
                     juce::Justification::centredLeft, 1);

    auto noteArea = bounds.reduced(7.0f).withTrimmedTop(20.0f);
    constexpr auto lowPitch = 36;
    constexpr auto highPitch = 84;
    for (const auto& note : clip.notes)
    {
        const auto x = noteArea.getX() + static_cast<float>(
            note.startBeats / std::max(0.001, clip.lengthBeats)) * noteArea.getWidth();
        const auto width = std::max(2.0f, static_cast<float>(
            note.durationBeats / std::max(0.001, clip.lengthBeats)) * noteArea.getWidth());
        const auto pitch = juce::jlimit(lowPitch, highPitch, note.noteNumber);
        const auto y = noteArea.getBottom() - static_cast<float>(pitch - lowPitch + 1)
            / static_cast<float>(highPitch - lowPitch + 1) * noteArea.getHeight();
        const auto height = std::max(2.0f,
            noteArea.getHeight() / static_cast<float>(highPitch - lowPitch + 1));
        g.fillRect(juce::Rectangle<float>(x, y, width, height));
    }
}

void MidiClipView::mouseDown(const juce::MouseEvent& event)
{
    dragStartBounds = getBounds();
    if (event.x <= 7)
        dragMode = DragMode::trimStart;
    else if (event.x >= getWidth() - 7)
        dragMode = DragMode::trimEnd;
    else
        dragMode = DragMode::move;
    if (onSelected)
        onSelected(*this);
}

void MidiClipView::mouseDrag(const juce::MouseEvent& event)
{
    if (onGesture)
        onGesture(*this, dragMode, event.getDistanceFromDragStartX(),
                  event.getDistanceFromDragStartY(), false, event.mods.isAltDown());
}

void MidiClipView::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasDraggedSinceMouseDown() && onGesture)
        onGesture(*this, dragMode, event.getDistanceFromDragStartX(),
                  event.getDistanceFromDragStartY(), true, event.mods.isAltDown());
}

void MidiClipView::mouseMove(const juce::MouseEvent& event)
{
    setMouseCursor(event.x <= 7 || event.x >= getWidth() - 7
        ? juce::MouseCursor::LeftRightResizeCursor
        : juce::MouseCursor::DraggingHandCursor);
}

void MidiClipView::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onOpenEditor)
        onOpenEditor(*this);
}

void MidiClipView::setSelected(bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}
}
