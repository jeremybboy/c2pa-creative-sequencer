#pragma once

#include "engine/ProjectEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace c2paseq
{
class MidiClipView final : public juce::Component
{
public:
    enum class DragMode { move, trimStart, trimEnd };

    MidiClipView(ArrangementMidiClipSnapshot clip, int trackIndex, juce::Colour colour);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void setSelected(bool);
    void setTrimPreview(double sourceOffsetBeats, double lengthBeats);
    void clearTrimPreview();

    [[nodiscard]] const juce::String& id() const noexcept { return clip.id; }
    [[nodiscard]] int track() const noexcept { return trackNumber; }
    [[nodiscard]] double startBeats() const noexcept { return clip.startBeats; }
    [[nodiscard]] double lengthBeats() const noexcept { return clip.lengthBeats; }
    [[nodiscard]] juce::Rectangle<int> gestureBounds() const noexcept { return dragStartBounds; }

    std::function<void(MidiClipView&)> onSelected;
    std::function<void(MidiClipView&)> onOpenEditor;
    std::function<void(MidiClipView&, DragMode, int, int, bool, bool)> onGesture;

private:
    ArrangementMidiClipSnapshot clip;
    int trackNumber = 0;
    juce::Colour clipColour;
    bool selected = false;
    std::optional<double> previewSourceOffsetBeats;
    std::optional<double> previewLengthBeats;
    DragMode dragMode = DragMode::move;
    juce::Rectangle<int> dragStartBounds;
};
}
