#pragma once

#include "engine/ProjectEngine.h"
#include "engine/SampleAuditionPlayer.h"
#include "export/ExportResult.h"
#include "midi/ComputerKeyboardMapping.h"
#include "timeline/TimelineGeometry.h"
#include "timeline/ArrangementSelection.h"
#include "ui/PlacesBrowser.h"
#include "ui/PlacesStore.h"
#include "ui/IconButton.h"
#include "ui/MidiClipView.h"
#include "ui/PianoRollView.h"
#include "ui/SequencerLookAndFeel.h"
#include "ui/WaveformView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <array>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <thread>
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
    explicit ArrangementView(AudioEngine& audioEngine,
                             std::function<void()> audioWMarkSetupRequest = {});
    ~ArrangementView() override;

    void audioWMarkSetupFinished(bool installed, const juce::String& message);
    [[nodiscard]] bool midiStemBounceActive() const noexcept { return midiStemBounceInProgress; }
    [[nodiscard]] bool offlineExportActive() const noexcept { return exportInProgress; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int x, int y) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

private:
    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void refreshTransport();
    void refreshTrackMeters();
    void showAudioSettings();
    void createProject();
    void openProject();
    void saveProject();
    void exportProject();
    void beginExportWithConfiguredSigner();
    void startBackgroundExport(const juce::File& destination);
    void bounceMidiStem(int trackIndex);
    void updateExportProgress(ExportStage);
    void completeBackgroundExport(ExportResult);
    void setExportInProgress(bool);
    void chooseSigningCredential(std::function<void()> continuation = {});
    void showSigningSettings();
    void showExportCompletion();
    void showExportCredentials();
    void showSelectedCredentials();
    void togglePlayback();
    void stopSampleAudition(const juce::String& message = {});
    void scanPlugins();
    void locatePlugin();
    void undoEdit();
    void redoEdit();
    void showAddTrackMenu();
    void addTrack(TrackType type);
    void requestDeleteTrack(int trackIndex);
    void deleteTrack(int trackIndex);
    void importAudioFiles(const juce::Array<juce::File>&, int x, int y);
    void rebuildArrangement();
    void layoutArrangement();
    void updateScrollRanges();
    void zoomBy(double factor, double anchorX);
    void handleWheel(const juce::MouseEvent&, const juce::MouseWheelDetails&);
    void selectClip(const juce::String& id);
    void showMidiClipCreationMenu(double seconds, int trackIndex);
    void createMidiClipFromSelection(int trackIndex);
    void openPianoRoll(const juce::String& clipId);
    void closePianoRoll();
    void refreshPianoRoll();
    void selectTrackForInput(int trackIndex);
    [[nodiscard]] int trackIndexForClip(const juce::String& clipId) const;
    void setComputerKeyboardEnabled(bool enabled);
    [[nodiscard]] bool handleComputerKeyboardKeyPress(const juce::KeyPress&);
    void releaseComputerKeyboardNotes();
    void allComputerKeyboardNotesOff();
    void selectAllClips();
    [[nodiscard]] std::vector<juce::String> selectedClipVector() const;
    [[nodiscard]] bool textEditorHasFocus() const;
    void copySelection();
    void cutSelection();
    void pasteSelection();
    void duplicateSelection();
    void loopFromSelection();
    void setTimeSelection(ArrangementTimeSelection selection);
    void clearTimeSelection();
    void handleClipGesture(WaveformView&, WaveformView::DragMode,
                           int deltaX, int deltaY, bool finished, bool bypassSnap);
    void handleMidiClipGesture(MidiClipView&, MidiClipView::DragMode,
                               int deltaX, int deltaY, bool finished, bool bypassSnap);
    void showProjectResult(const juce::Result&, const juce::String& successMessage);
    void applyEditResult(const juce::Result&, const juce::String& successMessage);
    void deferTrackEdit(std::function<juce::Result(AudioEngine&)>,
                        juce::String successMessage);
    void beginTrackMixGesture(int trackIndex);
    void previewTrackGain(int trackIndex, double value);
    void previewTrackPan(int trackIndex, double value);
    void endTrackMixGesture(int trackIndex, const juce::String& successMessage);
    [[nodiscard]] int trackAt(int parentY) const;
    [[nodiscard]] double timeAt(int parentX, bool snap) const;
    [[nodiscard]] juce::Colour colourForTrack(int index) const;

    AudioEngine& audioEngine;
    std::function<void()> requestAudioWMarkSetup;
    SequencerLookAndFeel lookAndFeel;
    SampleAuditionPlayer sampleAudition;
    PlacesStore placesStore;
    PlacesBrowser browser;
    std::unique_ptr<TimelineSurface> timelineSurface;
    std::unique_ptr<PianoRollView> pianoRoll;
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
    juce::TextButton signingButton { "Signing" };
    juce::TextButton audioSoftBindingButton { "Audio SB" };
    juce::TextButton fingerprintButton { "FP SB" };
    IconButton undoButton { "Undo", IconButton::Icon::undo };
    IconButton redoButton { "Redo", IconButton::Icon::redo };
    IconButton playPause { "Play Pause", IconButton::Icon::play };
    IconButton stop { "Stop", IconButton::Icon::stop };
    IconButton loop { "Loop", IconButton::Icon::loop };
    IconButton zoomOut { "Zoom Out", IconButton::Icon::zoomOut };
    IconButton zoomIn { "Zoom In", IconButton::Icon::zoomIn };
    IconButton audioSettings { "Audio Settings", IconButton::Icon::audio };
    IconButton computerKeyboard { "Computer MIDI Keyboard", IconButton::Icon::keyboard };
    juce::TextButton addTrackButton { "+ Track" };
    juce::Label position;
    juce::Label projectName;
    juce::Label status;
    juce::Slider bpm;
    std::array<int, 4> toolbarDividers {};

    std::set<juce::String> selectedClipIds;
    struct ActiveComputerNote
    {
        int trackIndex = -1;
        int noteNumber = -1;
    };
    std::map<int, ActiveComputerNote> activeComputerNotes;
    int selectedTrackIndex = -1;
    int computerKeyboardOctave = 0;
    ArrangementTimeSelection timeSelection;
    std::optional<double> insertionPointSeconds;
    std::optional<int> insertionPointTrack;
    juce::String projectMessage;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::optional<ExportResult> lastExport;
    std::vector<ArrangementTrackSnapshot> snapshots;
    std::vector<std::unique_ptr<WaveformView>> waveformViews;
    std::vector<std::unique_ptr<MidiClipView>> midiClipViews;
    std::vector<std::unique_ptr<TrackHeaderView>> trackHeaders;
    juce::String openPianoRollClipId;
    std::atomic_bool exportCancellationRequested { false };
    bool exportInProgress = false;
    bool midiStemBounceInProgress = false;
    std::thread exportThread;
};
}
