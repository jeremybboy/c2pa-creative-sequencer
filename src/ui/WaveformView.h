#pragma once

#include "provenance/ProvenanceModel.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <optional>

namespace c2paseq
{
class WaveformView final : public juce::Component,
                           private juce::ChangeListener
{
public:
    enum class DragMode { move, trimStart, trimEnd, selectTime };

    WaveformView(juce::AudioFormatManager& formatManager,
                 juce::AudioThumbnailCache& thumbnailCache,
                 juce::File audioFile,
                 juce::String clipName,
                 juce::String clipId,
                 int trackIndex,
                 double startSeconds,
                 double sourceOffsetSeconds,
                 double lengthSeconds,
                 juce::Colour colour,
                 ProvenanceStatus provenanceStatus);

    void paint(juce::Graphics& graphics) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void setSelected(bool shouldBeSelected);
    void setTimeSelection(double startSeconds, double endSeconds, bool active);
    void setTrimPreview(double sourceOffsetSeconds, double lengthSeconds);
    void clearTrimPreview();

    [[nodiscard]] const juce::String& id() const noexcept { return identifier; }
    [[nodiscard]] int track() const noexcept { return trackNumber; }
    [[nodiscard]] double start() const noexcept { return timelineStart; }
    [[nodiscard]] double offset() const noexcept { return sourceOffset; }
    [[nodiscard]] double length() const noexcept { return duration; }
    [[nodiscard]] double sourceLength() const noexcept
    {
        return thumbnail.getTotalLength() > 0.0 ? thumbnail.getTotalLength()
                                                 : sourceOffset + duration;
    }
    [[nodiscard]] juce::Rectangle<int> gestureBounds() const noexcept { return dragStartBounds; }
    [[nodiscard]] int gestureStartX() const noexcept { return dragStartLocalX; }

    std::function<void(WaveformView&)> onSelected;
    std::function<void(WaveformView&, DragMode, int, int, bool, bool)> onGesture;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    juce::AudioThumbnail thumbnail;
    juce::File file;
    juce::String name;
    juce::String identifier;
    int trackNumber = 0;
    double timelineStart = 0.0;
    double sourceOffset = 0.0;
    double duration = 0.0;
    std::optional<double> previewSourceOffset;
    std::optional<double> previewDuration;
    juce::Colour clipColour;
    ProvenanceStatus provenance = ProvenanceStatus::noCredentials;
    bool selected = false;
    bool timeSelectionActive = false;
    double timeSelectionStart = 0.0;
    double timeSelectionEnd = 0.0;
    DragMode dragMode = DragMode::move;
    juce::Rectangle<int> dragStartBounds;
    int dragStartLocalX = 0;
};
}
