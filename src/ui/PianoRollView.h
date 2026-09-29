#pragma once

#include "engine/ProjectEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <set>

namespace c2paseq
{
class PianoRollView final : public juce::Component
{
public:
    PianoRollView();

    void setClip(ArrangementMidiClipSnapshot clip);
    void setPlayheadBeat(double absoluteBeat);
    [[nodiscard]] const juce::String& clipId() const noexcept { return midiClip.id; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onClose;
    std::function<void(int noteNumber, double startBeats,
                       double durationBeats, int velocity)> onAddNote;
    std::function<void(const juce::String& noteId, int noteNumber,
                       double startBeats, double durationBeats,
                       int velocity)> onUpdateNote;
    std::function<void(const std::vector<ArrangementMidiNoteSnapshot>&)> onInsertNotes;
    std::function<void(const std::vector<juce::String>&)> onDeleteNotes;
    std::function<void(const juce::String&)> onStatus;

private:
    enum class DragMode { none, move, resize, velocity };

    [[nodiscard]] juce::Rectangle<float> gridBounds() const;
    [[nodiscard]] juce::Rectangle<float> velocityBounds() const;
    [[nodiscard]] juce::Rectangle<float> noteBounds(
        const ArrangementMidiNoteSnapshot&) const;
    [[nodiscard]] const ArrangementMidiNoteSnapshot* noteAt(juce::Point<float>) const;
    [[nodiscard]] int pitchAt(float y) const;
    [[nodiscard]] double beatAt(float x, bool snap = true) const;
    [[nodiscard]] int velocityAt(float y) const;
    [[nodiscard]] ArrangementMidiNoteSnapshot draggedNoteFor(
        const juce::MouseEvent&) const;
    [[nodiscard]] juce::String songPositionLabel(double localBeat) const;
    [[nodiscard]] const ArrangementMidiNoteSnapshot* selectedNote() const;
    [[nodiscard]] int highestVisiblePitch() const noexcept;
    [[nodiscard]] double visibleBeatLength() const noexcept;
    void copySelectedNotes(bool cut);
    void insertClipboard(bool duplicate);
    [[nodiscard]] std::vector<juce::String> selectedNoteIds() const;

    ArrangementMidiClipSnapshot midiClip;
    std::set<juce::String> selectedNotes;
    std::vector<ArrangementMidiNoteSnapshot> noteClipboard;
    double clipboardSpanBeats = 0.0;
    double pasteCursorBeats = 0.0;
    bool selectInsertedNotesOnNextUpdate = false;
    ArrangementMidiNoteSnapshot dragNote;
    std::optional<ArrangementMidiNoteSnapshot> dragPreview;
    DragMode dragMode = DragMode::none;
    juce::Point<float> dragStart;
    double horizontalZoom = 1.0;
    double visibleStartBeat = 0.0;
    double playheadBeat = -1.0;
    int lowestVisiblePitch = 48;
    int visiblePitchCount = 24;

    static constexpr int headerHeight = 38;
    static constexpr int timeRulerHeight = 24;
    static constexpr int keyboardWidth = 58;
    static constexpr int velocityHeight = 82;
    static constexpr double gridStepBeats = 0.25;
};
}
