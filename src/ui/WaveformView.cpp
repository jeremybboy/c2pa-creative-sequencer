#include "WaveformView.h"

namespace c2paseq
{
WaveformView::WaveformView(juce::AudioFormatManager& formatManager,
                           juce::AudioThumbnailCache& thumbnailCache,
                           juce::File audioFile,
                           juce::String clipName,
                           juce::String clipId,
                           int trackIndex,
                           double startSeconds,
                           double sourceOffsetSeconds,
                           double lengthSeconds,
                           juce::Colour colour,
                           ProvenanceStatus provenanceStatus)
    : thumbnail(256, formatManager, thumbnailCache),
      file(std::move(audioFile)),
      name(std::move(clipName)),
      identifier(std::move(clipId)),
      trackNumber(trackIndex),
      timelineStart(startSeconds),
      sourceOffset(sourceOffsetSeconds),
      duration(lengthSeconds),
      clipColour(colour),
      provenance(provenanceStatus)
{
    thumbnail.addChangeListener(this);
    thumbnail.setSource(new juce::FileInputSource(file));
}

void WaveformView::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(clipColour.withAlpha(0.82f));
    graphics.fillRect(bounds);
    graphics.setColour(selected ? juce::Colour::fromRGB(255, 213, 92)
                                : clipColour.brighter(0.35f));
    graphics.drawRect(bounds.reduced(0.5f), selected ? 2.0f : 1.0f);

    if (timeSelectionActive)
    {
        const auto overlapStart = std::max(timelineStart, timeSelectionStart);
        const auto overlapEnd = std::min(timelineStart + duration, timeSelectionEnd);
        if (overlapEnd > overlapStart)
        {
            const auto left = static_cast<float>((overlapStart - timelineStart)
                / duration * getWidth());
            const auto right = static_cast<float>((overlapEnd - timelineStart)
                / duration * getWidth());
            graphics.setColour(juce::Colour::fromRGB(255, 213, 92).withAlpha(0.22f));
            graphics.fillRect(juce::Rectangle<float>(left, 20.0f,
                std::max(1.0f, right - left), std::max(1.0f, bounds.getHeight() - 20.0f)));
            graphics.setColour(juce::Colour::fromRGB(255, 224, 125).withAlpha(0.9f));
            graphics.drawVerticalLine(juce::roundToInt(left), 20.0f, bounds.getHeight());
            graphics.drawVerticalLine(juce::roundToInt(right), 20.0f, bounds.getHeight());
        }
    }

    auto waveformBounds = bounds.reduced(8.0f).withTrimmedTop(18.0f);
    graphics.setColour(juce::Colour::fromRGB(224, 218, 207));
    if (thumbnail.getTotalLength() > 0.0)
        thumbnail.drawChannels(graphics, waveformBounds.toNearestInt(), sourceOffset,
                               sourceOffset + duration, 1.0f);
    else
        graphics.drawText("Building waveform...", waveformBounds,
                          juce::Justification::centred);

    graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    auto titleBounds = getLocalBounds().reduced(8).removeFromTop(16);
    auto badgeBounds = titleBounds.removeFromRight(48);
    const auto badgeText = provenance == ProvenanceStatus::valid ? "CC"
        : provenance == ProvenanceStatus::noCredentials ? "No CC" : "CC ?";
    graphics.setColour(provenance == ProvenanceStatus::valid
        ? juce::Colour::fromRGB(112, 226, 154) : juce::Colour::fromRGB(235, 224, 203));
    graphics.drawFittedText(badgeText, badgeBounds, juce::Justification::centredRight, 1);
    graphics.setColour(juce::Colour::fromRGB(224, 218, 207));
    graphics.drawFittedText(name, titleBounds,
                            juce::Justification::centredLeft, 1);
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    dragStartBounds = getBounds();
    dragStartLocalX = event.x;
    if (event.x <= 7)
        dragMode = DragMode::trimStart;
    else if (event.x >= getWidth() - 7)
        dragMode = DragMode::trimEnd;
    else if (event.y >= 20)
        dragMode = DragMode::selectTime;
    else
        dragMode = DragMode::move;
    if (onSelected)
        onSelected(*this);
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (onGesture)
        onGesture(*this, dragMode, event.getDistanceFromDragStartX(),
                  event.getDistanceFromDragStartY(), false, event.mods.isAltDown());
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasDraggedSinceMouseDown() && onGesture)
        onGesture(*this, dragMode, event.getDistanceFromDragStartX(),
                  event.getDistanceFromDragStartY(), true, event.mods.isAltDown());
}

void WaveformView::mouseMove(const juce::MouseEvent& event)
{
    if (event.x <= 7 || event.x >= getWidth() - 7)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (event.y < 20)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::IBeamCursor);
}

void WaveformView::setSelected(bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void WaveformView::setTimeSelection(double startSeconds, double endSeconds, bool active)
{
    timeSelectionStart = startSeconds;
    timeSelectionEnd = endSeconds;
    timeSelectionActive = active;
    repaint();
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}
}
