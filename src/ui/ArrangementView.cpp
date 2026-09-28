#include "ArrangementView.h"

#include "engine/AudioEngine.h"
#include "transport/TransportFormatting.h"
#include "ui/ArrangementShortcuts.h"
#include "ui/TrackHeaderView.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace c2paseq
{
namespace
{
constexpr int topBarHeight = 42;
constexpr int statusHeight = 22;
constexpr int browserWidth = 230;
constexpr int trackHeaderWidth = 205;
constexpr int rulerHeight = 28;
constexpr int trackHeight = 82;
constexpr int scrollBarSize = 14;
constexpr int minimumVisibleTracks = 4;
}

class TimelineSurface final : public juce::Component
{
public:
    void setState(TimelineGeometry newGeometry, int newTrackCount,
                  const TransportSnapshot& transport, double newVerticalOffset,
                  ArrangementTimeSelection newSelection)
    {
        geometry = newGeometry;
        trackCount = std::max(minimumVisibleTracks, newTrackCount);
        setTransportState(transport);
        verticalOffset = newVerticalOffset;
        selection = newSelection;
        repaint();
    }

    void setTransportState(const TransportSnapshot& transport)
    {
        playhead = transport.positionSeconds;
        looping = transport.looping;
        loopStart = transport.loopStartSeconds;
        loopEnd = transport.loopEndSeconds;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour::fromRGB(78, 81, 85));
        g.setColour(juce::Colour::fromRGB(48, 50, 53));
        g.fillRect(0, 0, getWidth(), rulerHeight);

        if (looping && loopEnd > loopStart + 0.001)
        {
            const auto startX = static_cast<float>(geometry.timeToX(loopStart));
            const auto endX = static_cast<float>(geometry.timeToX(loopEnd));
            const auto visible = juce::Rectangle<float>(0.0f, 0.0f,
                static_cast<float>(getWidth()), static_cast<float>(rulerHeight));
            const auto range = juce::Rectangle<float>(startX, 0.0f,
                std::max(1.0f, endX - startX), static_cast<float>(rulerHeight))
                    .getIntersection(visible);
            g.setColour(juce::Colour::fromRGB(197, 151, 49).withAlpha(0.28f));
            g.fillRect(range);
            g.setColour(juce::Colour::fromRGB(255, 213, 92));
            g.drawLine(startX, static_cast<float>(rulerHeight - 3), endX,
                       static_cast<float>(rulerHeight - 3), 2.0f);
        }

        const auto beatDuration = geometry.beatSeconds();
        const auto visibleStart = geometry.scrollSeconds;
        const auto visibleEnd = geometry.xToTime(static_cast<double>(getWidth()));
        const auto firstBeat = static_cast<int>(std::floor(visibleStart / beatDuration));
        const auto lastBeat = static_cast<int>(std::ceil(visibleEnd / beatDuration));

        for (int beat = firstBeat; beat <= lastBeat; ++beat)
        {
            const auto x = static_cast<float>(geometry.timeToX(beat * beatDuration));
            const auto isBar = beat % TimelineGeometry::beatsPerBar == 0;
            if (! isBar && geometry.pixelsPerSecond * beatDuration < 22.0)
                continue;
            g.setColour(isBar ? juce::Colour::fromRGB(48, 51, 54)
                              : juce::Colour::fromRGB(67, 70, 74));
            g.drawVerticalLine(static_cast<int>(std::round(x)),
                               isBar ? 0.0f : static_cast<float>(rulerHeight),
                               static_cast<float>(getHeight()));
            if (isBar)
            {
                g.setColour(juce::Colour::fromRGB(211, 214, 217));
                g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
                g.drawText(juce::String(beat / TimelineGeometry::beatsPerBar + 1),
                           static_cast<int>(x) + 5, 0, 48, rulerHeight,
                           juce::Justification::centredLeft);
            }
        }

        for (int track = 0; track <= trackCount; ++track)
        {
            const auto y = rulerHeight + track * trackHeight
                - static_cast<int>(std::round(verticalOffset));
            g.setColour(juce::Colour::fromRGB(55, 58, 61));
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
        }

        if (selection.isValid())
        {
            const auto startX = static_cast<float>(geometry.timeToX(selection.startSeconds));
            const auto endX = static_cast<float>(geometry.timeToX(selection.endSeconds));
            const auto top = static_cast<float>(rulerHeight + selection.firstTrack * trackHeight)
                - static_cast<float>(verticalOffset);
            const auto bottom = static_cast<float>(rulerHeight
                + (selection.lastTrack + 1) * trackHeight) - static_cast<float>(verticalOffset);
            auto selectedBounds = juce::Rectangle<float>(startX, top,
                std::max(1.0f, endX - startX), std::max(1.0f, bottom - top))
                    .getIntersection(getLocalBounds().toFloat());
            g.setColour(juce::Colour::fromRGB(255, 213, 92).withAlpha(0.13f));
            g.fillRect(selectedBounds);
            g.setColour(juce::Colour::fromRGB(255, 224, 125).withAlpha(0.75f));
            g.drawRect(selectedBounds, 1.0f);
        }

        const auto playheadX = static_cast<int>(std::round(geometry.timeToX(playhead)));
        if (juce::isPositiveAndBelow(playheadX, getWidth()))
        {
            g.setColour(juce::Colour::fromRGB(239, 91, 73));
            g.fillRect(playheadX, 0, 2, getHeight());
            juce::Path marker;
            marker.addTriangle(static_cast<float>(playheadX - 5), 0.0f,
                               static_cast<float>(playheadX + 7), 0.0f,
                               static_cast<float>(playheadX + 1), 8.0f);
            g.fillPath(marker);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartTime = geometry.xToTime(static_cast<double>(event.x));
        dragStartTrack = trackForY(event.y);
        dragBypassSnap = event.mods.isAltDown();
        if (event.y < rulerHeight)
        {
            if (onBackgroundClick) onBackgroundClick();
            if (onSeek) onSeek(dragStartTime);
        }
        else if (onBackgroundClick)
        {
            onBackgroundClick();
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragStartTrack < 0 || ! onTimeSelection)
            return;
        onTimeSelection(dragStartTime,
            geometry.xToTime(static_cast<double>(event.x)), dragStartTrack,
            trackForY(event.y), dragBypassSnap, false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (dragStartTrack < 0)
            return;
        const auto end = geometry.xToTime(static_cast<double>(event.x));
        if (event.mouseWasDraggedSinceMouseDown() && onTimeSelection)
            onTimeSelection(dragStartTime, end, dragStartTrack, trackForY(event.y),
                            dragBypassSnap, true);
        else if (onInsertionPoint)
            onInsertionPoint(dragStartTime, dragStartTrack, dragBypassSnap);
        dragStartTrack = -1;
    }

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        if (onWheel)
            onWheel(event, wheel);
    }

    std::function<void(double)> onSeek;
    std::function<void()> onBackgroundClick;
    std::function<void(double, double, int, int, bool, bool)> onTimeSelection;
    std::function<void(double, int, bool)> onInsertionPoint;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

private:
    [[nodiscard]] int trackForY(int y) const noexcept
    {
        return juce::jlimit(0, std::max(0, trackCount - 1),
            static_cast<int>(std::floor(
                (y - rulerHeight + verticalOffset) / static_cast<double>(trackHeight))));
    }

    TimelineGeometry geometry;
    int trackCount = minimumVisibleTracks;
    double playhead = 0.0;
    bool looping = false;
    double loopStart = 0.0;
    double loopEnd = 0.0;
    double verticalOffset = 0.0;
    ArrangementTimeSelection selection;
    double dragStartTime = 0.0;
    int dragStartTrack = -1;
    bool dragBypassSnap = false;
};

ArrangementView::ArrangementView(AudioEngine& engine)
    : audioEngine(engine),
      sampleAudition(engine.audioDeviceManager(), engine.audioFormatManager()),
      browser(placesStore), timelineSurface(std::make_unique<TimelineSurface>())
{
    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);

    browser.onStatus = [this](const juce::String& message)
    {
        projectMessage = message;
        refreshTransport();
    };
    browser.onPreviewRequested = [this](const juce::File& file)
    {
        const auto result = sampleAudition.preview(file);
        if (result.failed())
            projectMessage = "Preview error: " + result.getErrorMessage();
        else
            projectMessage = "Previewing " + file.getFileName();
        browser.setPreviewState(sampleAudition.currentFile(), sampleAudition.isPlaying());
        refreshTransport();
    };
    browser.onPreviewStopRequested = [this] { stopSampleAudition("Preview stopped"); };
    browser.onAudioDragStarted = [this] { stopSampleAudition("Preview stopped for drag"); };

    timelineSurface->onSeek = [this](double seconds)
    {
        audioEngine.seek(geometry.snapToBeat(seconds));
        refreshTransport();
    };
    timelineSurface->onBackgroundClick = [this]
    {
        selectClip({});
        clearTimeSelection();
    };
    timelineSurface->onTimeSelection = [this](double start, double end,
                                               int firstTrack, int lastTrack,
                                               bool bypass, bool)
    {
        if (! bypass)
        {
            start = geometry.snapToBeat(start);
            end = geometry.snapToBeat(end);
        }
        setTimeSelection(ArrangementTimeSelection::between(
            start, end, firstTrack, lastTrack));
    };
    timelineSurface->onInsertionPoint = [this](double seconds, int track, bool bypass)
    {
        insertionPointSeconds = bypass ? seconds : geometry.snapToBeat(seconds);
        insertionPointTrack = track;
        audioEngine.seek(*insertionPointSeconds);
        clearTimeSelection();
        refreshTransport();
    };
    timelineSurface->onWheel = [this](const auto& event, const auto& wheel)
    {
        handleWheel(event, wheel);
    };

    newProject.onClick = [this] { createProject(); };
    openProjectButton.onClick = [this] { openProject(); };
    saveProjectButton.onClick = [this] { saveProject(); };
    exportButton.onClick = [this]
    {
        if (exportInProgress)
        {
            exportCancellationRequested.store(true);
            projectMessage = "Cancelling export…";
            status.setText(projectMessage, juce::dontSendNotification);
            return;
        }
        exportProject();
    };
    credentialsButton.onClick = [this] { showSelectedCredentials(); };
    signingButton.onClick = [this] { showSigningSettings(); };
    audioSoftBindingButton.setClickingTogglesState(true);
    audioSoftBindingButton.onClick = [this]
    {
        audioEngine.setSoftBindingEnabled(audioSoftBindingButton.getToggleState());
        projectMessage = "Audio soft binding "
            + juce::String(audioEngine.softBindingEnabled() ? "enabled" : "disabled")
            + " | Runtime: " + audioEngine.watermarkStatus();
        refreshTransport();
    };
    fingerprintButton.setClickingTogglesState(true);
    fingerprintButton.onClick = [this]
    {
        audioEngine.setFingerprintEnabled(fingerprintButton.getToggleState());
        projectMessage = "Fingerprint soft binding "
            + juce::String(audioEngine.fingerprintEnabled() ? "enabled" : "disabled")
            + " | Runtime: " + audioEngine.fingerprintStatus();
        refreshTransport();
    };
    undoButton.onClick = [this] { undoEdit(); };
    redoButton.onClick = [this] { redoEdit(); };
    playPause.onClick = [this] { togglePlayback(); };
    stop.onClick = [this] { audioEngine.stop(); refreshTransport(); };
    loop.setClickingTogglesState(true);
    loop.onClick = [this]
    {
        const auto enabled = loop.getToggleState();
        if (enabled && (timeSelection.isValid() || ! selectedClipIds.empty()))
            loopFromSelection();
        else
            audioEngine.setLooping(enabled);
        const auto snapshot = audioEngine.transportSnapshot();
        projectMessage = snapshot.looping
            ? "Loop: " + juce::String(snapshot.loopStartSeconds, 3)
                + " - " + juce::String(snapshot.loopEndSeconds, 3) + " s"
            : "Loop off";
        refreshTransport();
    };
    zoomOut.onClick = [this] { zoomBy(0.8, timelineBounds.getWidth() * 0.5); };
    zoomIn.onClick = [this] { zoomBy(1.25, timelineBounds.getWidth() * 0.5); };
    audioSettings.onClick = [this] { showAudioSettings(); };
    addTrackButton.onClick = [this] { showAddTrackMenu(); };

    undoButton.setTooltip("Undo (Command-Z)");
    redoButton.setTooltip("Redo (Shift-Command-Z)");
    playPause.setTooltip("Play or pause (Space)");
    stop.setTooltip("Stop and return to the beginning");
    loop.setTooltip("Loop the selected clip or full arrangement");
    zoomOut.setTooltip("Zoom out (Command-minus)");
    zoomIn.setTooltip("Zoom in (Command-plus)");
    audioSettings.setTooltip("Audio device settings");
    addTrackButton.setTooltip("Add an Audio or MIDI track");

    position.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    position.setJustificationType(juce::Justification::centred);
    projectName.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    projectName.setJustificationType(juce::Justification::centred);
    status.setFont(juce::FontOptions(11.5f));
    status.setColour(juce::Label::textColourId, juce::Colour::fromRGB(173, 179, 184));

    bpm.setSliderStyle(juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    bpm.setRange(transport::minimumBpm, transport::maximumBpm, 0.1);
    bpm.setTextValueSuffix(" BPM");
    bpm.onValueChange = [this]
    {
        audioEngine.setBpm(bpm.getValue());
        geometry.bpm = bpm.getValue();
        updateScrollRanges();
        layoutArrangement();
    };

    const std::array<juce::Button*, 17> buttons {
        &newProject, &openProjectButton, &saveProjectButton, &exportButton,
        &credentialsButton, &signingButton, &audioSoftBindingButton, &fingerprintButton,
        &undoButton, &redoButton, &playPause, &stop, &loop,
        &zoomOut, &zoomIn, &audioSettings, &addTrackButton
    };
    for (auto* button : buttons)
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(66, 70, 74));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(231, 233, 235));
        addAndMakeVisible(*button);
    }
    loop.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(197, 151, 49));
    audioSoftBindingButton.setColour(juce::TextButton::buttonOnColourId,
                                     juce::Colour::fromRGB(42, 139, 157));
    fingerprintButton.setColour(juce::TextButton::buttonOnColourId,
                                juce::Colour::fromRGB(80, 125, 183));

    addAndMakeVisible(browser);
    addAndMakeVisible(*timelineSurface);
    addAndMakeVisible(headerContainer);
    addAndMakeVisible(horizontalScroll);
    addAndMakeVisible(verticalScroll);
    addAndMakeVisible(position);
    addAndMakeVisible(projectName);
    addAndMakeVisible(status);
    addAndMakeVisible(bpm);
    horizontalScroll.addListener(this);
    verticalScroll.addListener(this);

    geometry.pixelsPerSecond = audioEngine.timelinePixelsPerSecond();
    geometry.scrollSeconds = audioEngine.timelineScrollSeconds();
    refreshTransport();
    rebuildArrangement();
    startTimerHz(30);
}

ArrangementView::~ArrangementView()
{
    exportCancellationRequested.store(true);
    if (exportThread.joinable()) exportThread.join();
    horizontalScroll.removeListener(this);
    verticalScroll.removeListener(this);
    setLookAndFeel(nullptr);
}

void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(45, 48, 51));
    g.setColour(juce::Colour::fromRGB(34, 36, 38));
    g.fillRect(0, 0, getWidth(), topBarHeight);
    g.setColour(juce::Colour::fromRGB(76, 80, 84));
    g.drawHorizontalLine(topBarHeight - 1, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(getHeight() - statusHeight, 0.0f, static_cast<float>(getWidth()));
    g.setColour(juce::Colour::fromRGB(91, 97, 102));
    for (const auto x : toolbarDividers)
        g.drawVerticalLine(x, 8.0f, static_cast<float>(topBarHeight - 8));
}

void ArrangementView::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop(topBarHeight).reduced(6, 5);
    status.setBounds(area.removeFromBottom(statusHeight).reduced(8, 1));
    browser.setBounds(area.removeFromLeft(browserWidth));

    auto placeButton = [&](juce::Component& component, int width)
    {
        component.setBounds(top.removeFromLeft(width).reduced(1));
    };
    auto divider = [this, &top](std::size_t index)
    {
        top.removeFromLeft(6);
        toolbarDividers[index] = top.getX() - 3;
    };

    placeButton(newProject, 38); placeButton(openProjectButton, 40);
    placeButton(saveProjectButton, 38); placeButton(exportButton, 48);
    divider(0);
    placeButton(undoButton, 30); placeButton(redoButton, 30);
    divider(1);
    placeButton(playPause, 32); placeButton(stop, 32); placeButton(loop, 32);
    placeButton(position, 84);
    bpm.setBounds(top.removeFromLeft(110).reduced(2, 0));
    divider(2);
    placeButton(credentialsButton, 78); placeButton(signingButton, 58);
    placeButton(audioSoftBindingButton, 62); placeButton(fingerprintButton, 52);

    audioSettings.setBounds(top.removeFromRight(32).reduced(1));
    zoomIn.setBounds(top.removeFromRight(30).reduced(1));
    zoomOut.setBounds(top.removeFromRight(30).reduced(1));
    addTrackButton.setBounds(top.removeFromRight(58).reduced(1));
    toolbarDividers[3] = top.getRight() - 3;
    top.removeFromRight(6);
    projectName.setBounds(top.reduced(8, 0));

    verticalScroll.setBounds(area.removeFromRight(scrollBarSize));
    headerBounds = area.removeFromRight(trackHeaderWidth);
    headerContainer.setBounds(headerBounds);
    horizontalScroll.setBounds(area.removeFromBottom(scrollBarSize));
    timelineBounds = area;
    timelineSurface->setBounds(timelineBounds);

    updateScrollRanges();
    layoutArrangement();
}

bool ArrangementView::keyPressed(const juce::KeyPress& key)
{
    if (exportInProgress)
        return true;

    const auto command = commandForKeyPress(key);
    if (textEditorHasFocus()
        && (command == ArrangementCommand::selectAll
            || command == ArrangementCommand::copy
            || command == ArrangementCommand::cut
            || command == ArrangementCommand::paste))
        return false;

    switch (command)
    {
        case ArrangementCommand::togglePlayPause: togglePlayback(); return true;
        case ArrangementCommand::save: saveProject(); return true;
        case ArrangementCommand::undo: undoEdit(); return true;
        case ArrangementCommand::redo: redoEdit(); return true;
        case ArrangementCommand::selectAll: selectAllClips(); return true;
        case ArrangementCommand::copy: copySelection(); return true;
        case ArrangementCommand::cut: cutSelection(); return true;
        case ArrangementCommand::paste: pasteSelection(); return true;
        case ArrangementCommand::duplicateClip: duplicateSelection(); return true;
        case ArrangementCommand::splitClip:
            if (selectedClipIds.size() == 1)
                applyEditResult(audioEngine.splitClip(*selectedClipIds.begin(),
                    audioEngine.transportSnapshot().positionSeconds), "Split clip");
            return true;
        case ArrangementCommand::deleteClip:
            if (! selectedClipIds.empty())
            {
                const auto ids = selectedClipVector();
                selectedClipIds.clear();
                applyEditResult(audioEngine.deleteClips(ids), "Deleted selected clips");
            }
            return true;
        case ArrangementCommand::loopSelection: loopFromSelection(); return true;
        case ArrangementCommand::zoomIn:
            zoomBy(1.25, timelineBounds.getWidth() * 0.5); return true;
        case ArrangementCommand::zoomOut:
            zoomBy(0.8, timelineBounds.getWidth() * 0.5); return true;
        case ArrangementCommand::none: break;
    }
    return false;
}

bool ArrangementView::isInterestedInFileDrag(const juce::StringArray& files)
{
    return ! files.isEmpty() && std::all_of(files.begin(), files.end(), [](const auto& path)
    {
        return AudioEngine::isSupportedAudioFile(juce::File(path));
    });
}

void ArrangementView::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::Array<juce::File> audioFiles;
    for (const auto& path : files) audioFiles.add(juce::File(path));
    importAudioFiles(audioFiles, x, y);
}

bool ArrangementView::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith("c2paseq-audio:");
}

void ArrangementView::itemDropped(const SourceDetails& details)
{
    const auto path = details.description.toString().fromFirstOccurrenceOf(
        "c2paseq-audio:", false, false);
    importAudioFiles({ juce::File(path) }, details.localPosition.x, details.localPosition.y);
}

void ArrangementView::timerCallback()
{
    if (sampleAudition.currentFile() != juce::File() && ! sampleAudition.isPlaying())
        stopSampleAudition("Preview finished");
    refreshTransport();
    refreshTrackMeters();
}

void ArrangementView::refreshTrackMeters()
{
    const auto playing = audioEngine.transportSnapshot().playing;
    const auto anySolo = std::any_of(snapshots.begin(), snapshots.end(),
        [](const auto& track) { return track.soloed; });
    const auto count = std::min(trackHeaders.size(), snapshots.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto peak = audioEngine.trackLevelSnapshot(static_cast<int>(index));
        const auto& track = snapshots[index];
        const auto audible = playing && ! track.muted && (! anySolo || track.soloed);
        trackHeaders[index]->setMeterPeak(peak, audible);
    }
}

void ArrangementView::scrollBarMoved(juce::ScrollBar* bar, double start)
{
    if (bar == &horizontalScroll) geometry.scrollSeconds = std::max(0.0, start);
    if (bar == &verticalScroll) verticalOffset = std::max(0.0, start);
    audioEngine.setTimelineView(geometry.pixelsPerSecond, geometry.scrollSeconds);
    layoutArrangement();
}

void ArrangementView::refreshTransport()
{
    const auto snapshot = audioEngine.transportSnapshot();
    playPause.setIcon(snapshot.playing ? IconButton::Icon::pause : IconButton::Icon::play);
    playPause.setTooltip(snapshot.playing ? "Pause (Space)" : "Play (Space)");
    loop.setToggleState(snapshot.looping, juce::dontSendNotification);
    position.setText(transport::formatPosition(snapshot.positionSeconds).c_str(),
                     juce::dontSendNotification);
    bpm.setValue(snapshot.bpm, juce::dontSendNotification);
    geometry.bpm = snapshot.bpm;
    projectName.setText(audioEngine.projectName(), juce::dontSendNotification);
    saveProjectButton.setEnabled(audioEngine.hasProject());
    exportButton.setEnabled(audioEngine.hasProject());
    loop.setEnabled(audioEngine.hasProject());
    addTrackButton.setEnabled(audioEngine.hasProject());
    credentialsButton.setEnabled(selectedClipIds.size() == 1);
    undoButton.setEnabled(audioEngine.canUndo());
    redoButton.setEnabled(audioEngine.canRedo());
    const auto prefix = projectMessage.isNotEmpty() ? projectMessage + "  |  " : juce::String();
    status.setText(prefix + audioEngine.status(), juce::dontSendNotification);
    timelineSurface->setTransportState(snapshot);
}

void ArrangementView::createProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Create Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Untitled Project.c2paseq"), "*.c2paseq");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& c)
        {
            if (safe == nullptr || c.getResult() == juce::File()) return;
            auto folder = c.getResult();
            if (! folder.hasFileExtension("c2paseq")) folder = folder.withFileExtension("c2paseq");
            safe->showProjectResult(safe->audioEngine.createProject(
                folder, folder.getFileNameWithoutExtension()),
                "Created " + folder.getFileNameWithoutExtension());
            safe->fileChooser.reset();
        });
}

void ArrangementView::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Open Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.c2paseq");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectDirectories
                               | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& c)
        {
            if (safe == nullptr || c.getResult() == juce::File()) return;
            const auto folder = c.getResult();
            const auto result = safe->audioEngine.openProject(folder);
            if (result.wasOk())
            {
                safe->geometry.pixelsPerSecond = safe->audioEngine.timelinePixelsPerSecond();
                safe->geometry.scrollSeconds = safe->audioEngine.timelineScrollSeconds();
            }
            safe->showProjectResult(result, "Opened " + folder.getFileNameWithoutExtension());
            safe->fileChooser.reset();
        });
}

void ArrangementView::saveProject()
{
    audioEngine.setTimelineView(geometry.pixelsPerSecond, geometry.scrollSeconds);
    showProjectResult(audioEngine.saveProject(), "Project saved");
}

void ArrangementView::exportProject()
{
    if (! audioEngine.signingConfigured())
    {
        const auto options = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Signing Credential Required")
            .withMessage("A C2PA signing credential is required to generate Content Credentials.")
            .withButton("Choose Credential")
            .withButton("Cancel")
            .withAssociatedComponent(this);
        juce::NativeMessageBox::showAsync(options,
            [safe = juce::Component::SafePointer<ArrangementView>(this)](int result)
            {
                if (safe != nullptr && result == 0)
                    safe->chooseSigningCredential([safe]
                    {
                        if (safe != nullptr)
                            safe->beginExportWithConfiguredSigner();
                    });
            });
        return;
    }
    beginExportWithConfiguredSigner();
}

void ArrangementView::beginExportWithConfiguredSigner()
{
    fileChooser = std::make_unique<juce::FileChooser>("Export WAV",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory)
            .getChildFile(audioEngine.projectName() + " Export.wav"), "*.wav");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& c)
        {
            if (safe == nullptr || c.getResult() == juce::File()) return;
            auto destination = c.getResult();
            if (! destination.hasFileExtension("wav"))
                destination = destination.withFileExtension("wav");
            safe->fileChooser.reset();
            safe->startBackgroundExport(destination);
        });
}

void ArrangementView::startBackgroundExport(const juce::File& destination)
{
    if (exportThread.joinable()) exportThread.join();
    exportCancellationRequested.store(false);
    setExportInProgress(true);
    updateExportProgress(ExportStage::planning);
    const auto safe = juce::Component::SafePointer<ArrangementView>(this);
    exportThread = std::thread([this, safe, destination]
    {
        auto progress = [safe](ExportStage stage)
        {
            juce::MessageManager::callAsync([safe, stage]
            {
                if (safe != nullptr) safe->updateExportProgress(stage);
            });
        };
        auto exportResult = audioEngine.exportMix(destination, std::move(progress), [this]
        {
            return exportCancellationRequested.load();
        });
        juce::MessageManager::callAsync([safe, completed = std::move(exportResult)]() mutable
        {
            if (safe != nullptr) safe->completeBackgroundExport(std::move(completed));
        });
    });
}

void ArrangementView::updateExportProgress(ExportStage stage)
{
    switch (stage)
    {
        case ExportStage::planning: projectMessage = "Preparing export…"; break;
        case ExportStage::audioRender: projectMessage = "Rendering audio…"; break;
        case ExportStage::watermarkEmbedding: projectMessage = "Embedding AudioWMark…"; break;
        case ExportStage::fingerprintComputation:
            projectMessage = "Computing audio fingerprint…"; break;
        case ExportStage::creatingContentCredentials:
            projectMessage = "Creating Content Credentials…"; break;
        case ExportStage::signingAndEmbedding: projectMessage = "Signing…"; break;
        case ExportStage::finalValidation: projectMessage = "Validating…"; break;
        case ExportStage::publicationOutbox:
            projectMessage = "Preparing recovery publication…"; break;
        case ExportStage::fileCommit: projectMessage = "Committing export…"; break;
        case ExportStage::cancelled: projectMessage = "Export cancelled"; break;
        case ExportStage::complete: projectMessage = "Export complete"; break;
        case ExportStage::signingConfiguration: projectMessage = "Checking signing…"; break;
    }
    status.setText(projectMessage, juce::dontSendNotification);
    repaint();
}

void ArrangementView::completeBackgroundExport(ExportResult result)
{
    if (exportThread.joinable()) exportThread.join();
    setExportInProgress(false);
    if (result.result.failed())
    {
        lastExport.reset();
        projectMessage = result.stage == ExportStage::cancelled
            ? "Export cancelled" : "Export failed: " + result.result.getErrorMessage();
        refreshTransport();
        if (result.stage != ExportStage::cancelled)
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon, "Export Failed",
                result.result.getErrorMessage(), "OK", this);
        return;
    }
    lastExport = std::move(result);
    projectMessage = lastExport->externallyTrusted
        ? "Export complete | Content Credentials attached | Validation successful"
        : "Export complete | Content Credentials attached | Asset integrity validated | External trust issue";
    if (lastExport->softBindingEnabled)
        projectMessage += " | Audio soft binding " + lastExport->softBindingPayloadHex;
    if (lastExport->fingerprintEnabled)
        projectMessage += " | Fingerprint soft binding " + lastExport->fingerprintValueHex;
    refreshTransport();
    showExportCompletion();
}

void ArrangementView::setExportInProgress(bool active)
{
    exportInProgress = active;
    if (active) stopTimer(); else startTimerHz(30);
    exportButton.setButtonText(active ? "Cancel" : "Export");
    for (auto* component : { static_cast<juce::Component*>(&newProject),
                             static_cast<juce::Component*>(&openProjectButton),
                             static_cast<juce::Component*>(&saveProjectButton),
                             static_cast<juce::Component*>(&credentialsButton),
                             static_cast<juce::Component*>(&signingButton),
                             static_cast<juce::Component*>(&audioSoftBindingButton),
                             static_cast<juce::Component*>(&fingerprintButton),
                             static_cast<juce::Component*>(&undoButton),
                             static_cast<juce::Component*>(&redoButton),
                             static_cast<juce::Component*>(&playPause),
                             static_cast<juce::Component*>(&stop),
                             static_cast<juce::Component*>(&loop),
                             static_cast<juce::Component*>(&bpm) })
        component->setEnabled(! active);
    browser.setEnabled(! active);
    timelineSurface->setEnabled(! active);
    headerContainer.setEnabled(! active);
    horizontalScroll.setEnabled(! active);
    verticalScroll.setEnabled(! active);
    zoomOut.setEnabled(! active);
    zoomIn.setEnabled(! active);
    audioSettings.setEnabled(! active);
    exportButton.setEnabled(true);
}

void ArrangementView::chooseSigningCredential(std::function<void()> continuation)
{
    fileChooser = std::make_unique<juce::FileChooser>("Choose C2PA Signing Credential",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Downloads"), "*.pem");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<ArrangementView>(this),
         afterConfigured = std::move(continuation)](const juce::FileChooser& chooser) mutable
        {
            if (safe == nullptr || chooser.getResult() == juce::File()) return;
            const auto credential = chooser.getResult();
            safe->fileChooser.reset();
            const auto result = safe->audioEngine.configureSigningCredential(credential);
            if (result.failed())
            {
                safe->projectMessage = result.getErrorMessage();
                safe->refreshTransport();
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon, "Invalid Signing Credential",
                    result.getErrorMessage(), "OK", safe.getComponent());
                return;
            }
            safe->projectMessage = "C2PA signing credential configured";
            safe->refreshTransport();
            if (afterConfigured) afterConfigured();
        });
}

void ArrangementView::showSigningSettings()
{
    juce::AlertWindow::showYesNoCancelBox(
        juce::MessageBoxIconType::InfoIcon, "Signing Credential",
        audioEngine.signingCredentialStatus()
            + "\n\nThe private key is never stored in projects or shown here.",
        "Replace", "Remove", "Done", this,
        juce::ModalCallbackFunction::create(
            [safe = juce::Component::SafePointer<ArrangementView>(this)](int result)
            {
                if (safe == nullptr) return;
                if (result == 1)
                {
                    safe->chooseSigningCredential();
                    return;
                }
                if (result == 2)
                {
                    const auto removal = safe->audioEngine.removeSigningCredential();
                    safe->projectMessage = removal.wasOk()
                        ? "Machine-local signing credential removed"
                        : removal.getErrorMessage();
                    safe->refreshTransport();
                }
            }));
}

void ArrangementView::showExportCompletion()
{
    if (! lastExport.has_value()) return;
    const auto trustSummary = lastExport->externallyTrusted
        ? juce::String("\nExternal trust: recognized")
        : juce::String("\nExternal trust: validation issue (see credentials)");
    juce::AlertWindow::showYesNoCancelBox(
        juce::MessageBoxIconType::InfoIcon, "Export Complete",
        "Content Credentials attached\nAsset integrity validation successful"
            + trustSummary + "\n\n"
            + lastExport->outputFile.getFullPathName(),
        "View Credentials", "Reveal File", "Done", this,
        juce::ModalCallbackFunction::create(
            [safe = juce::Component::SafePointer<ArrangementView>(this)](int result)
            {
                if (safe == nullptr || ! safe->lastExport.has_value()) return;
                if (result == 1) safe->showExportCredentials();
                if (result == 2) safe->lastExport->outputFile.revealToUser();
            }));
}

void ArrangementView::showExportCredentials()
{
    if (! lastExport.has_value()) return;
    const auto& result = *lastExport;
    auto details = "File: " + result.outputFile.getFileName()
        + "\nClaim generator: " + result.outputProvenance.claimGenerator
        + "\nStatus: " + provenanceStatusLabel(result.outputProvenance.status)
        + "\nValidation: " + result.outputProvenance.validationSummary
        + "\nIngredients: " + juce::String(result.ingredients.size());
    if (result.softBindingEnabled)
        details += "\nAudio soft binding: Published"
            "\nAlgorithm: " + juce::String(audioWMarkAlgorithm.data())
            + "\nBinding: " + result.softBindingPayloadHex
            + "\nPublication package: " + result.publicationPackage.getFullPathName()
            + "\nAudioWMark embed: " + juce::String(result.watermarkEmbedSeconds, 3) + " s"
            + "\nTotal export: " + juce::String(result.totalSeconds, 3) + " s";
    if (result.fingerprintEnabled)
        details += "\nFingerprint soft binding: Published"
            "\nAlgorithm: " + juce::String(audfprintAlgorithm.data())
            + "\nRegistration: " + result.fingerprintValueHex
            + "\nFingerprint time: " + juce::String(result.fingerprintSeconds, 3) + " s"
            + "\nPublication package: " + result.publicationPackage.getFullPathName();
    if (result.outputProvenance.activeManifest.isNotEmpty())
        details += "\nManifest: " + result.outputProvenance.activeManifest;
    for (const auto& ingredient : result.ingredients)
        details += "\n- " + ingredient.title + ": "
            + provenanceStatusLabel(ingredient.provenance.status);
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Exported Content Credentials", details, "OK", this);
}

void ArrangementView::showSelectedCredentials()
{
    for (const auto& track : snapshots)
        for (const auto& clip : track.clips)
            if (selectedClipIds.contains(clip.id))
            {
                const auto& info = clip.provenance;
                auto details = "File: " + clip.mediaFile.getFileName()
                    + "\nContent Credentials: " + (info.c2paPresent ? "Present" : "Not present")
                    + "\nStatus: " + provenanceStatusLabel(info.status);
                if (info.activeManifest.isNotEmpty())
                    details += "\nActive manifest: " + info.activeManifest;
                if (info.claimGenerator.isNotEmpty())
                    details += "\nGenerator: " + info.claimGenerator;
                if (info.signer.isNotEmpty())
                    details += "\nSigner: " + info.signer;
                if (info.validationSummary.isNotEmpty())
                    details += "\nValidation: " + info.validationSummary;
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Content Credentials", details);
                return;
            }
}

void ArrangementView::togglePlayback()
{
    if (audioEngine.transportSnapshot().playing)
        audioEngine.pause();
    else
        audioEngine.play();
    refreshTransport();
}

void ArrangementView::stopSampleAudition(const juce::String& message)
{
    const auto hadPreview = sampleAudition.currentFile() != juce::File();
    sampleAudition.stop();
    browser.setPreviewState(juce::File(), false);
    if (hadPreview && message.isNotEmpty())
        projectMessage = message;
    refreshTransport();
}

void ArrangementView::undoEdit()
{
    if (audioEngine.undo())
    {
        projectMessage = "Undo";
        rebuildArrangement();
    }
}

void ArrangementView::redoEdit()
{
    if (audioEngine.redo())
    {
        projectMessage = "Redo";
        rebuildArrangement();
    }
}

void ArrangementView::showAddTrackMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Audio Track");
    menu.addItem(2, "MIDI Track");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(addTrackButton),
        [safe = juce::Component::SafePointer<ArrangementView>(this)](int result)
        {
            if (safe == nullptr)
                return;
            if (result == 1) safe->addTrack(TrackType::audio);
            if (result == 2) safe->addTrack(TrackType::midi);
        });
}

void ArrangementView::addTrack(TrackType type)
{
    const auto result = type == TrackType::midi ? audioEngine.addMidiTrack()
                                                : audioEngine.addAudioTrack();
    applyEditResult(result, type == TrackType::midi ? "Added MIDI track"
                                                     : "Added audio track");
}

void ArrangementView::requestDeleteTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(snapshots.size())))
        return;

    const auto& track = snapshots[static_cast<std::size_t>(trackIndex)];
    if (track.clips.empty() && track.midiClipCount == 0 && ! track.plugin.has_value())
    {
        deleteTrack(trackIndex);
        return;
    }

    auto contents = juce::String();
    if (! track.clips.empty())
        contents << juce::String(static_cast<int>(track.clips.size()))
                 << (track.clips.size() == 1 ? " clip" : " clips");
    if (track.midiClipCount > 0)
    {
        if (contents.isNotEmpty())
            contents << " and ";
        contents << juce::String(static_cast<int>(track.midiClipCount))
                 << (track.midiClipCount == 1 ? " MIDI clip" : " MIDI clips");
    }
    if (track.plugin.has_value())
    {
        if (contents.isNotEmpty())
            contents << " and ";
        contents << "the loaded VST3";
    }

    const auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle(track.type == TrackType::midi ? "Delete MIDI Track?"
                                                 : "Delete Audio Track?")
        .withMessage("\"" + track.name + "\" contains " + contents
                     + ". Deleting the track removes this project data from the arrangement."
                       " Original audio media files remain unchanged.")
        .withButton("Delete Track")
        .withButton("Cancel")
        .withAssociatedComponent(this);
    juce::NativeMessageBox::showAsync(options,
        [safe = juce::Component::SafePointer<ArrangementView>(this), trackIndex](int result)
        {
            if (safe != nullptr && result == 0)
                safe->deleteTrack(trackIndex);
        });
}

void ArrangementView::deleteTrack(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, static_cast<int>(snapshots.size())))
    {
        const auto& deleted = snapshots[static_cast<std::size_t>(trackIndex)];
        for (const auto& clip : deleted.clips)
            selectedClipIds.erase(clip.id);
    }
    applyEditResult(audioEngine.deleteTrack(trackIndex), "Deleted track");
}

void ArrangementView::importAudioFiles(const juce::Array<juce::File>& files, int x, int y)
{
    stopSampleAudition();
    if (! audioEngine.hasProject())
    {
        projectMessage = "Create or open a project before placing audio";
        refreshTransport();
        return;
    }
    if (! timelineBounds.contains(x, y))
    {
        projectMessage = "Drop audio inside an arrangement track";
        refreshTransport();
        return;
    }
    const auto targetTrack = trackAt(y);
    const auto targetTime = timeAt(x, true);
    int imported = 0;
    for (const auto& file : files)
    {
        const auto result = audioEngine.importAudio(file, targetTrack, targetTime);
        if (result.failed())
        {
            projectMessage = "Import error: " + result.getErrorMessage();
            rebuildArrangement();
            return;
        }
        ++imported;
    }
    projectMessage = "Placed " + juce::String(imported)
        + (imported == 1 ? " audio clip" : " audio clips");
    rebuildArrangement();
}

void ArrangementView::rebuildArrangement()
{
    waveformViews.clear();
    trackHeaders.clear();
    snapshots = audioEngine.arrangementSnapshot();

    for (std::size_t trackIndex = 0; trackIndex < snapshots.size(); ++trackIndex)
    {
        const auto colour = colourForTrack(static_cast<int>(trackIndex));
        auto header = std::make_unique<TrackHeaderView>(static_cast<int>(trackIndex));
        header->setState(snapshots[trackIndex].name, snapshots[trackIndex].gainDb,
                         snapshots[trackIndex].pan, snapshots[trackIndex].muted,
                         snapshots[trackIndex].soloed, snapshots[trackIndex].type, colour);
        header->setPluginState(snapshots[trackIndex].plugin,
                               audioEngine.availableVst3Plugins());
        header->onNameChanged = [this](int index, const auto& name)
        {
            deferTrackEdit([index, name](AudioEngine& engine)
                { return engine.setTrackName(index, name); }, "Renamed track");
        };
        header->onMuteChanged = [this](int index, bool value)
        {
            deferTrackEdit([index, value](AudioEngine& engine)
                { return engine.setTrackMute(index, value); },
                value ? "Muted track" : "Unmuted track");
        };
        header->onSoloChanged = [this](int index, bool value)
        {
            deferTrackEdit([index, value](AudioEngine& engine)
                { return engine.setTrackSolo(index, value); },
                value ? "Soloed track" : "Unsoloed track");
        };
        header->onGainChanged = [this](int index, double value)
        {
            deferTrackEdit([index, value](AudioEngine& engine)
                { return engine.setTrackGain(index, value); }, "Changed track gain");
        };
        header->onGainGestureStart = [this](int index) { beginTrackMixGesture(index); };
        header->onGainPreview = [this](int index, double value)
        {
            previewTrackGain(index, value);
        };
        header->onGainGestureEnd = [this](int index)
        {
            endTrackMixGesture(index, "Changed track gain");
        };
        header->onPanChanged = [this](int index, double value)
        {
            deferTrackEdit([index, value](AudioEngine& engine)
                { return engine.setTrackPan(index, value); }, "Changed track pan");
        };
        header->onPanGestureStart = [this](int index) { beginTrackMixGesture(index); };
        header->onPanPreview = [this](int index, double value)
        {
            previewTrackPan(index, value);
        };
        header->onPanGestureEnd = [this](int index)
        {
            endTrackMixGesture(index, "Changed track pan");
        };
        header->onScanPlugins = [this] { scanPlugins(); };
        header->onLoadPlugin = [this](int index, const auto& identifier)
        {
            applyEditResult(audioEngine.loadTrackPlugin(index, identifier),
                            "Loaded VST3");
        };
        header->onOpenPlugin = [this](int index)
        {
            const auto result = audioEngine.openTrackPluginEditor(index);
            projectMessage = result.wasOk() ? "Opened VST3 editor"
                                             : "VST3 error: " + result.getErrorMessage();
            refreshTransport();
        };
        header->onBypassPlugin = [this](int index, bool bypassed)
        {
            applyEditResult(audioEngine.setTrackPluginBypassed(index, bypassed),
                            bypassed ? "Bypassed VST3" : "Enabled VST3");
        };
        header->onRemovePlugin = [this](int index)
        {
            applyEditResult(audioEngine.removeTrackPlugin(index), "Removed VST3");
        };
        header->onDeleteTrack = [this](int index) { requestDeleteTrack(index); };
        headerContainer.addAndMakeVisible(*header);
        trackHeaders.push_back(std::move(header));

        for (const auto& clip : snapshots[trackIndex].clips)
        {
            auto view = std::make_unique<WaveformView>(
                audioEngine.audioFormatManager(), audioEngine.audioThumbnailCache(),
                clip.mediaFile, clip.name, clip.id, static_cast<int>(trackIndex),
                clip.startSeconds, clip.sourceOffsetSeconds, clip.lengthSeconds, colour,
                clip.provenance.status);
            view->setSelected(selectedClipIds.contains(clip.id));
            view->setTimeSelection(timeSelection.startSeconds, timeSelection.endSeconds,
                timeSelection.isValid()
                    && static_cast<int>(trackIndex) >= timeSelection.firstTrack
                    && static_cast<int>(trackIndex) <= timeSelection.lastTrack);
            view->onSelected = [this](auto& selected) { selectClip(selected.id()); };
            view->onGesture = [this](auto& selected, auto mode, int dx, int dy,
                                     bool finished, bool bypass)
            { handleClipGesture(selected, mode, dx, dy, finished, bypass); };
            timelineSurface->addAndMakeVisible(*view);
            waveformViews.push_back(std::move(view));
        }
    }
    updateScrollRanges();
    layoutArrangement();
    refreshTransport();
}

void ArrangementView::scanPlugins()
{
    projectMessage = "Scanning standard macOS VST3 folders...";
    refreshTransport();
    const auto result = audioEngine.scanVst3Plugins();
    projectMessage = result.wasOk()
        ? "VST3 scan complete: "
            + juce::String(static_cast<int>(audioEngine.availableVst3Plugins().size()))
            + " plug-ins cached"
        : "VST3 scan error: " + result.getErrorMessage();
    rebuildArrangement();
}

void ArrangementView::layoutArrangement()
{
    timelineSurface->setState(geometry, static_cast<int>(snapshots.size()),
        audioEngine.transportSnapshot(), verticalOffset, timeSelection);
    for (auto& view : waveformViews)
    {
        const auto x = juce::roundToInt(geometry.timeToX(view->start()));
        const auto y = rulerHeight + view->track() * trackHeight
            - juce::roundToInt(verticalOffset) + 4;
        const auto width = std::max(6, juce::roundToInt(view->length() * geometry.pixelsPerSecond));
        view->setBounds(x, y, width, trackHeight - 8);
        view->setVisible(view->getBounds().intersects(timelineSurface->getLocalBounds()));
    }
    for (std::size_t index = 0; index < trackHeaders.size(); ++index)
    {
        const auto y = rulerHeight + static_cast<int>(index) * trackHeight
            - juce::roundToInt(verticalOffset);
        trackHeaders[index]->setBounds(0, y, headerContainer.getWidth(), trackHeight);
        trackHeaders[index]->setVisible(
            trackHeaders[index]->getBounds().intersects(headerContainer.getLocalBounds()));
    }
    repaint();
}

void ArrangementView::updateScrollRanges()
{
    auto maxEnd = geometry.barSeconds() * 8.0;
    for (const auto& track : snapshots)
        for (const auto& clip : track.clips)
            maxEnd = std::max(maxEnd, clip.startSeconds + clip.lengthSeconds);
    timelineDuration = std::max(geometry.barSeconds() * 64.0,
                                maxEnd + geometry.barSeconds() * 8.0);
    const auto visibleSeconds = timelineBounds.getWidth() > 0
        ? timelineBounds.getWidth() / geometry.pixelsPerSecond : 1.0;
    geometry.scrollSeconds = juce::jlimit(0.0, std::max(0.0, timelineDuration - visibleSeconds),
                                          geometry.scrollSeconds);
    horizontalScroll.setRangeLimits(0.0, timelineDuration);
    horizontalScroll.setCurrentRange(geometry.scrollSeconds,
                                     std::min(visibleSeconds, timelineDuration));

    const auto totalTrackHeight = std::max(minimumVisibleTracks,
        static_cast<int>(snapshots.size())) * trackHeight;
    const auto visibleTrackHeight = std::max(1, timelineBounds.getHeight() - rulerHeight);
    verticalOffset = juce::jlimit(0.0,
        std::max(0.0, static_cast<double>(totalTrackHeight - visibleTrackHeight)), verticalOffset);
    verticalScroll.setRangeLimits(0.0, static_cast<double>(std::max(totalTrackHeight, visibleTrackHeight)));
    verticalScroll.setCurrentRange(verticalOffset, static_cast<double>(visibleTrackHeight));
}

void ArrangementView::zoomBy(double factor, double anchorX)
{
    geometry.zoomAround(geometry.pixelsPerSecond * factor, anchorX);
    audioEngine.setTimelineView(geometry.pixelsPerSecond, geometry.scrollSeconds);
    updateScrollRanges();
    layoutArrangement();
}

void ArrangementView::handleWheel(const juce::MouseEvent& event,
                                  const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown())
    {
        const auto dominantDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX)
            ? wheel.deltaY : wheel.deltaX;
        const auto factor = juce::jlimit(0.75, 1.33,
            std::exp(static_cast<double>(dominantDelta) * 0.7));
        if (std::abs(dominantDelta) > 0.0001f)
            zoomBy(factor, event.position.x);
        return;
    }
    if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        horizontalScroll.setCurrentRangeStart(geometry.scrollSeconds
            - (wheel.deltaX + wheel.deltaY) * 3.0);
    else
        verticalScroll.setCurrentRangeStart(verticalOffset - wheel.deltaY * trackHeight * 2.0);
}

void ArrangementView::mouseMagnify(const juce::MouseEvent& event, float scaleFactor)
{
    if (scaleFactor > 0.0f && std::abs(scaleFactor - 1.0f) > 0.0001f)
        zoomBy(juce::jlimit(0.75, 1.33, static_cast<double>(scaleFactor)),
               event.position.x);
}

void ArrangementView::selectClip(const juce::String& id)
{
    selectedClipIds.clear();
    if (id.isNotEmpty())
        selectedClipIds.insert(id);
    if (id.isNotEmpty())
        clearTimeSelection();
    for (auto& view : waveformViews)
        view->setSelected(selectedClipIds.contains(view->id()));
    grabKeyboardFocus();
}

void ArrangementView::selectAllClips()
{
    clearTimeSelection();
    selectedClipIds.clear();
    for (const auto& track : snapshots)
        for (const auto& clip : track.clips)
            selectedClipIds.insert(clip.id);
    for (auto& view : waveformViews)
        view->setSelected(selectedClipIds.contains(view->id()));
    projectMessage = "Selected " + juce::String(static_cast<int>(selectedClipIds.size()))
        + " clips";
    refreshTransport();
}

std::vector<juce::String> ArrangementView::selectedClipVector() const
{
    return { selectedClipIds.begin(), selectedClipIds.end() };
}

bool ArrangementView::textEditorHasFocus() const
{
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    return dynamic_cast<juce::TextEditor*>(focused) != nullptr;
}

void ArrangementView::copySelection()
{
    const auto result = timeSelection.isValid()
        ? audioEngine.copyTimeRange(timeSelection)
        : audioEngine.copyClips(selectedClipVector());
    projectMessage = result.wasOk() ? "Copied selection"
                                    : "Copy error: " + result.getErrorMessage();
    refreshTransport();
}

void ArrangementView::cutSelection()
{
    const auto result = timeSelection.isValid()
        ? audioEngine.cutTimeRange(timeSelection)
        : audioEngine.cutClips(selectedClipVector());
    if (result.wasOk())
    {
        selectedClipIds.clear();
        clearTimeSelection();
    }
    applyEditResult(result, "Cut selection");
}

void ArrangementView::pasteSelection()
{
    const auto destination = timeSelection.isValid() ? timeSelection.startSeconds
        : insertionPointSeconds.value_or(audioEngine.transportSnapshot().positionSeconds);
    const auto destinationTrack = timeSelection.isValid() ? timeSelection.firstTrack
        : insertionPointTrack.value_or(-1);
    const auto result = audioEngine.pasteClipboard(destination, destinationTrack);
    if (result.wasOk())
    {
        selectedClipIds.clear();
        clearTimeSelection();
        insertionPointSeconds = destination;
    }
    applyEditResult(result, "Pasted selection");
}

void ArrangementView::duplicateSelection()
{
    const auto result = timeSelection.isValid()
        ? audioEngine.duplicateTimeRange(timeSelection)
        : audioEngine.duplicateClips(selectedClipVector());
    applyEditResult(result, "Duplicated selection");
}

void ArrangementView::loopFromSelection()
{
    auto start = 0.0;
    auto end = 0.0;
    auto found = false;
    if (timeSelection.isValid())
    {
        start = timeSelection.startSeconds;
        end = timeSelection.endSeconds;
        found = true;
    }
    else
    {
        start = std::numeric_limits<double>::max();
        for (const auto& track : snapshots)
            for (const auto& clip : track.clips)
                if (selectedClipIds.contains(clip.id))
                {
                    found = true;
                    start = std::min(start, clip.startSeconds);
                    end = std::max(end, clip.startSeconds + clip.lengthSeconds);
                }
    }

    if (! found)
    {
        projectMessage = "Select a time range or clip before using Loop Selection";
        refreshTransport();
        return;
    }
    const auto result = audioEngine.setLoopRangeAndEnable(start, end);
    projectMessage = result.wasOk()
        ? "Looped selection: " + juce::String(start, 3) + " - "
            + juce::String(end, 3) + " s"
        : "Loop error: " + result.getErrorMessage();
    refreshTransport();
}

void ArrangementView::setTimeSelection(ArrangementTimeSelection selection)
{
    timeSelection = selection;
    if (timeSelection.isValid())
        selectedClipIds.clear();
    for (auto& view : waveformViews)
    {
        view->setSelected(selectedClipIds.contains(view->id()));
        const auto onSelectedTrack = timeSelection.isValid()
            && view->track() >= timeSelection.firstTrack
            && view->track() <= timeSelection.lastTrack;
        view->setTimeSelection(timeSelection.startSeconds, timeSelection.endSeconds,
                               onSelectedTrack);
    }
    timelineSurface->setState(geometry, static_cast<int>(snapshots.size()),
        audioEngine.transportSnapshot(), verticalOffset, timeSelection);
    grabKeyboardFocus();
}

void ArrangementView::clearTimeSelection()
{
    timeSelection = {};
    for (auto& view : waveformViews)
        view->setTimeSelection(0.0, 0.0, false);
    if (timelineSurface != nullptr)
        timelineSurface->setState(geometry, static_cast<int>(snapshots.size()),
            audioEngine.transportSnapshot(), verticalOffset, timeSelection);
}

void ArrangementView::handleClipGesture(WaveformView& view, WaveformView::DragMode mode,
                                        int deltaX, int deltaY, bool finished, bool bypassSnap)
{
    if (! finished)
    {
        if (mode == WaveformView::DragMode::selectTime)
        {
            auto first = view.start() + view.gestureStartX() / geometry.pixelsPerSecond;
            auto second = first + deltaX / geometry.pixelsPerSecond;
            if (! bypassSnap)
            {
                first = geometry.snapToBeat(first);
                second = geometry.snapToBeat(second);
            }
            setTimeSelection(ArrangementTimeSelection::betweenWithin(
                first, second, view.start(), view.start() + view.length(), view.track()));
            return;
        }
        auto bounds = view.gestureBounds();
        if (mode == WaveformView::DragMode::move)
            bounds.translate(deltaX, deltaY);
        else if (mode == WaveformView::DragMode::trimStart)
        {
            const auto change = juce::jlimit(-bounds.getX(), bounds.getWidth() - 6, deltaX);
            bounds.setBounds(bounds.getX() + change, bounds.getY(),
                             bounds.getWidth() - change, bounds.getHeight());
        }
        else
            bounds.setWidth(std::max(6, bounds.getWidth() + deltaX));
        view.setBounds(bounds);
        return;
    }

    const auto deltaSeconds = deltaX / geometry.pixelsPerSecond;
    if (mode == WaveformView::DragMode::selectTime)
    {
        auto first = view.start() + view.gestureStartX() / geometry.pixelsPerSecond;
        auto second = first + deltaSeconds;
        if (! bypassSnap)
        {
            first = geometry.snapToBeat(first);
            second = geometry.snapToBeat(second);
        }
        setTimeSelection(ArrangementTimeSelection::betweenWithin(
            first, second, view.start(), view.start() + view.length(), view.track()));
        insertionPointSeconds = timeSelection.startSeconds;
        projectMessage = "Selected " + juce::String(timeSelection.endSeconds
            - timeSelection.startSeconds, 3) + " s";
        refreshTransport();
        return;
    }
    if (mode == WaveformView::DragMode::move)
    {
        auto start = std::max(0.0, view.start() + deltaSeconds);
        if (! bypassSnap) start = geometry.snapToBeat(start);
        const auto targetTrack = std::max(0, view.track()
            + static_cast<int>(std::round(deltaY / static_cast<double>(trackHeight))));
        applyEditResult(audioEngine.moveClip(view.id(), targetTrack, start), "Moved clip");
        return;
    }

    if (mode == WaveformView::DragMode::trimStart)
    {
        auto newStart = std::max(0.0, view.start() + deltaSeconds);
        if (! bypassSnap) newStart = geometry.snapToBeat(newStart);
        auto change = newStart - view.start();
        change = juce::jlimit(std::max(-view.offset(), -view.start()),
                              view.length() - 0.05, change);
        applyEditResult(audioEngine.trimClip(view.id(), view.start() + change,
            view.offset() + change, view.length() - change), "Trimmed clip start");
        return;
    }

    auto newEnd = view.start() + view.length() + deltaSeconds;
    if (! bypassSnap) newEnd = geometry.snapToBeat(newEnd);
    const auto maxLength = std::max(0.05, view.sourceLength() - view.offset());
    const auto newLength = juce::jlimit(0.05, maxLength, newEnd - view.start());
    applyEditResult(audioEngine.trimClip(view.id(), view.start(), view.offset(), newLength),
                    "Trimmed clip end");
}

void ArrangementView::showProjectResult(const juce::Result& result,
                                        const juce::String& successMessage)
{
    projectMessage = result.wasOk() ? successMessage : "Project error: " + result.getErrorMessage();
    if (result.wasOk()) rebuildArrangement();
    refreshTransport();
}

void ArrangementView::applyEditResult(const juce::Result& result,
                                      const juce::String& successMessage)
{
    projectMessage = result.wasOk() ? successMessage : "Edit error: " + result.getErrorMessage();
    rebuildArrangement();
}

void ArrangementView::deferTrackEdit(
    std::function<juce::Result(AudioEngine&)> edit,
    juce::String successMessage)
{
    juce::MessageManager::callAsync(
        [safe = juce::Component::SafePointer<ArrangementView>(this),
         pendingEdit = std::move(edit), message = std::move(successMessage)]
        {
            if (safe != nullptr)
                safe->applyEditResult(pendingEdit(safe->audioEngine), message);
        });
}

void ArrangementView::beginTrackMixGesture(int trackIndex)
{
    const auto result = audioEngine.beginTrackMixGesture(trackIndex);
    if (result.failed())
    {
        projectMessage = "Mixer error: " + result.getErrorMessage();
        refreshTransport();
    }
}

void ArrangementView::previewTrackGain(int trackIndex, double value)
{
    const auto result = audioEngine.previewTrackGain(trackIndex, value);
    if (result.wasOk()
        && juce::isPositiveAndBelow(trackIndex, static_cast<int>(snapshots.size())))
        snapshots[static_cast<std::size_t>(trackIndex)].gainDb = value;
    else if (result.failed())
        projectMessage = "Mixer error: " + result.getErrorMessage();
}

void ArrangementView::previewTrackPan(int trackIndex, double value)
{
    const auto result = audioEngine.previewTrackPan(trackIndex, value);
    if (result.wasOk()
        && juce::isPositiveAndBelow(trackIndex, static_cast<int>(snapshots.size())))
        snapshots[static_cast<std::size_t>(trackIndex)].pan = value;
    else if (result.failed())
        projectMessage = "Mixer error: " + result.getErrorMessage();
}

void ArrangementView::endTrackMixGesture(int trackIndex,
                                          const juce::String& successMessage)
{
    const auto result = audioEngine.endTrackMixGesture(trackIndex);
    projectMessage = result.wasOk() ? successMessage
                                    : "Mixer error: " + result.getErrorMessage();
    if (result.failed())
        juce::MessageManager::callAsync(
            [safe = juce::Component::SafePointer<ArrangementView>(this)]
            {
                if (safe != nullptr) safe->rebuildArrangement();
            });
    refreshTransport();
}

int ArrangementView::trackAt(int parentY) const
{
    const auto localY = parentY - timelineBounds.getY() - rulerHeight
        + static_cast<int>(std::round(verticalOffset));
    return juce::jlimit(0, 63, localY / trackHeight);
}

double ArrangementView::timeAt(int parentX, bool shouldSnap) const
{
    const auto seconds = geometry.xToTime(parentX - timelineBounds.getX());
    return shouldSnap ? geometry.snapToBeat(seconds) : seconds;
}

juce::Colour ArrangementView::colourForTrack(int index) const
{
    constexpr std::array colours {
        0xff4aa3b8u, 0xffd39a43u, 0xffbf6377u, 0xff7d9f65u,
        0xff8b78b8u, 0xffc1784fu, 0xff5c94c6u, 0xffa8749au
    };
    return juce::Colour(colours[static_cast<std::size_t>(index) % colours.size()]);
}

void ArrangementView::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(
        audioEngine.audioDeviceManager(), 0, 0, 1, 2, false, false, true, false);
    selector->setSize(520, 360);
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio Device";
    options.dialogBackgroundColour = juce::Colour::fromRGB(45, 48, 51);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}
}
