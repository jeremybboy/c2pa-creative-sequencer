#pragma once

#include "engine/ProjectEngine.h"
#include "timeline/TimelineGeometry.h"
#include "ui/PlacesBrowser.h"
#include "ui/PlacesStore.h"
#include "ui/WaveformView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace c2paseq
{
class AudioEngine;
class TimelineSurface;
class TrackHeaderView;

class ArrangementView final : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              public juce::DragAndDropTarget,
                              public juce::DragAndDropContainer,
                              private juce::ScrollBar::Listener,
                              private juce::Timer
{
public:
    explicit ArrangementView(AudioEngine& audioEngine);
    ~ArrangementView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int x, int y) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

private:
    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void refreshTransport();
    void showAudioSettings();
    void createProject();
    void openProject();
    void saveProject();
    void exportProject();
    void showSelectedCredentials();
    void togglePlayback();
    void undoEdit();
    void redoEdit();
    void importAudioFiles(const juce::Array<juce::File>&, int x, int y);
    void rebuildArrangement();
    void layoutArrangement();
    void updateScrollRanges();
    void zoomBy(double factor, double anchorX);
    void handleWheel(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void selectClip(const juce::String& id);
    void handleClipGesture(WaveformView&, WaveformView::DragMode,
                           int deltaX, int deltaY, bool finished, bool bypassSnap);
    void showProjectResult(const juce::Result&, const juce::String& successMessage);
    void applyEditResult(const juce::Result&, const juce::String& successMessage);
    void deferTrackEdit(std::function<juce::Result(AudioEngine&)>,
                        juce::String successMessage);
    [[nodiscard]] int trackAt(int parentY) const;
    [[nodiscard]] double timeAt(int parentX, bool snap) const;
    [[nodiscard]] juce::Colour colourForTrack(int index) const;

    AudioEngine& audioEngine;
    PlacesStore placesStore;
    PlacesBrowser browser;
    std::unique_ptr<TimelineSurface> timelineSurface;
    juce::Component headerContainer;
    juce::ScrollBar horizontalScroll { false };
    juce::ScrollBar verticalScroll { true };
    TimelineGeometry geometry;
    double verticalOffset = 0.0;
    double timelineDuration = 64.0;
    juce::Rectangle<int> timelineBounds;
    juce::Rectangle<int> headerBounds;

    juce::TextButton newProject { "New" };
    juce::TextButton openProjectButton { "Open" };
    juce::TextButton saveProjectButton { "Save" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton credentialsButton { "Credentials" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton playPause { "Play" };
    juce::TextButton stop { "Stop" };
    juce::TextButton loop { "Loop" };
    juce::TextButton zoomOut { "-" };
    juce::TextButton zoomIn { "+" };
    juce::TextButton audioSettings { "Audio" };
    juce::Label position;
    juce::Label projectName;
    juce::Label status;
    juce::Slider bpm;

    juce::String selectedClipId;
    juce::String projectMessage;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<ArrangementTrackSnapshot> snapshots;
    std::vector<std::unique_ptr<WaveformView>> waveformViews;
    std::vector<std::unique_ptr<TrackHeaderView>> trackHeaders;
};
}
