#include "ArrangementView.h"

#include "engine/AudioEngine.h"
#include "transport/TransportFormatting.h"
#include "ui/ArrangementShortcuts.h"
#include "ui/TrackHeaderView.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>

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
                  const TransportSnapshot& transport, double newVerticalOffset)
    {
        geometry = newGeometry;
        trackCount = std::max(minimumVisibleTracks, newTrackCount);
        setTransportState(transport);
        verticalOffset = newVerticalOffset;
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
        if (onBackgroundClick)
            onBackgroundClick();
        if (onSeek)
            onSeek(geometry.xToTime(static_cast<double>(event.x)));
    }

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        if (onWheel)
            onWheel(event, wheel);
    }

    std::function<void(double)> onSeek;
    std::function<void()> onBackgroundClick;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

private:
    TimelineGeometry geometry;
    int trackCount = minimumVisibleTracks;
    double playhead = 0.0;
    bool looping = false;
    double loopStart = 0.0;
    double loopEnd = 0.0;
    double verticalOffset = 0.0;
};

ArrangementView::ArrangementView(AudioEngine& engine)
    : audioEngine(engine), browser(placesStore), timelineSurface(std::make_unique<TimelineSurface>())
{
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);

    browser.onStatus = [this](const juce::String& message)
    {
        projectMessage = message;
        refreshTransport();
    };

    timelineSurface->onSeek = [this](double seconds)
    {
        audioEngine.seek(geometry.snapToBeat(seconds));
        refreshTransport();
    };
    timelineSurface->onBackgroundClick = [this] { selectClip({}); };
    timelineSurface->onWheel = [this](const auto& event, const auto& wheel)
    {
        handleWheel(event, wheel);
    };

    newProject.onClick = [this] { createProject(); };
    openProjectButton.onClick = [this] { openProject(); };
    saveProjectButton.onClick = [this] { saveProject(); };
    exportButton.onClick = [this] { exportProject(); };
    credentialsButton.onClick = [this] { showSelectedCredentials(); };
    signingButton.onClick = [this] { showSigningSettings(); };
    wavMarkButton.setClickingTogglesState(true);
    wavMarkButton.onClick = [this]
    {
        audioEngine.setSoftBindingEnabled(wavMarkButton.getToggleState());
        projectMessage = "WavMark recovery "
            + juce::String(audioEngine.softBindingEnabled() ? "enabled" : "disabled")
            + " | Runtime: " + audioEngine.watermarkStatus();
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
        audioEngine.setLooping(enabled, selectedClipId);
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

    for (auto* button : { &newProject, &openProjectButton, &saveProjectButton,
                          &exportButton, &credentialsButton, &signingButton, &wavMarkButton,
                          &undoButton, &redoButton, &playPause, &stop, &loop,
                          &zoomOut, &zoomIn, &audioSettings })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(66, 70, 74));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(231, 233, 235));
        addAndMakeVisible(*button);
    }
    loop.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(197, 151, 49));
    wavMarkButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colour::fromRGB(42, 139, 157));

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
    horizontalScroll.removeListener(this);
    verticalScroll.removeListener(this);
}

void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(45, 48, 51));
    g.setColour(juce::Colour::fromRGB(34, 36, 38));
    g.fillRect(0, 0, getWidth(), topBarHeight);
    g.setColour(juce::Colour::fromRGB(76, 80, 84));
    g.drawHorizontalLine(topBarHeight - 1, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(getHeight() - statusHeight, 0.0f, static_cast<float>(getWidth()));
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
    placeButton(newProject, 45); placeButton(openProjectButton, 48); placeButton(saveProjectButton, 46);
    placeButton(exportButton, 56); placeButton(credentialsButton, 82);
    placeButton(signingButton, 62);
    placeButton(wavMarkButton, 72);
    top.removeFromLeft(6);
    placeButton(undoButton, 48); placeButton(redoButton, 48);
    top.removeFromLeft(10);
    placeButton(playPause, 52); placeButton(stop, 44); placeButton(loop, 46);
    placeButton(position, 104);
    bpm.setBounds(top.removeFromLeft(138).reduced(2, 0));
    audioSettings.setBounds(top.removeFromRight(56).reduced(1));
    zoomIn.setBounds(top.removeFromRight(30).reduced(1));
    zoomOut.setBounds(top.removeFromRight(30).reduced(1));
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
    switch (commandForKeyPress(key))
    {
        case ArrangementCommand::togglePlayPause: togglePlayback(); return true;
        case ArrangementCommand::save: saveProject(); return true;
        case ArrangementCommand::undo: undoEdit(); return true;
        case ArrangementCommand::redo: redoEdit(); return true;
        case ArrangementCommand::duplicateClip:
            if (selectedClipId.isNotEmpty())
                applyEditResult(audioEngine.duplicateClip(selectedClipId), "Duplicated clip");
            return true;
        case ArrangementCommand::splitClip:
            if (selectedClipId.isNotEmpty())
                applyEditResult(audioEngine.splitClip(selectedClipId,
                    audioEngine.transportSnapshot().positionSeconds), "Split clip");
            return true;
        case ArrangementCommand::deleteClip:
            if (selectedClipId.isNotEmpty())
            {
                applyEditResult(audioEngine.deleteClip(selectedClipId), "Deleted clip");
                selectedClipId.clear();
            }
            return true;
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
    refreshTransport();
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
    playPause.setButtonText(snapshot.playing ? "Pause" : "Play");
    loop.setToggleState(snapshot.looping, juce::dontSendNotification);
    position.setText(transport::formatPosition(snapshot.positionSeconds).c_str(),
                     juce::dontSendNotification);
    bpm.setValue(snapshot.bpm, juce::dontSendNotification);
    geometry.bpm = snapshot.bpm;
    projectName.setText(audioEngine.projectName(), juce::dontSendNotification);
    saveProjectButton.setEnabled(audioEngine.hasProject());
    exportButton.setEnabled(audioEngine.hasProject());
    loop.setEnabled(audioEngine.hasProject());
    credentialsButton.setEnabled(selectedClipId.isNotEmpty());
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
            safe->projectMessage = safe->audioEngine.softBindingEnabled()
                ? "Rendering... WavMark embedding/verifying... C2PA signing... Storing recovery manifest..."
                : "Rendering mix... Creating Content Credentials... Signing... Embedding... Validating...";
            safe->refreshTransport();
            juce::MessageManager::callAsync([safe, destination]
            {
                if (safe == nullptr) return;
                auto result = safe->audioEngine.exportMix(destination);
                if (result.result.failed())
                {
                    safe->lastExport.reset();
                    safe->projectMessage = "Export failed: " + result.result.getErrorMessage();
                    safe->refreshTransport();
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon, "Export Failed",
                        result.result.getErrorMessage(), "OK", safe.getComponent());
                    return;
                }
                safe->lastExport = std::move(result);
                safe->projectMessage = safe->lastExport->externallyTrusted
                    ? "Export complete | Content Credentials attached | Validation successful"
                    : "Export complete | Content Credentials attached | Asset integrity validated | External trust issue";
                if (safe->lastExport->softBindingEnabled)
                    safe->projectMessage += " | WavMark recovery "
                        + safe->lastExport->softBindingPayloadHex;
                safe->refreshTransport();
                safe->showExportCompletion();
            });
        });
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
        details += "\nWavMark recovery: Enabled"
            "\nAlgorithm: " + juce::String(SoftBindingClaim::algorithm)
            + "\nPayload: " + result.softBindingPayloadHex
            + "\nMeasured SNR: " + juce::String(result.watermarkSnrDb, 2) + " dB"
            + "\nLocal manifest: " + result.softBindingManifestId;
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
            if (clip.id == selectedClipId)
            {
                const auto recovered = recoveredProvenance.find(clip.id.toStdString());
                const auto& info = recovered != recoveredProvenance.end()
                    ? recovered->second : clip.provenance;
                if (! info.c2paPresent && info.status == ProvenanceStatus::noCredentials)
                {
                    juce::AlertWindow::showOkCancelBox(
                        juce::MessageBoxIconType::InfoIcon, "Content Credentials",
                        "No embedded Content Credentials.\n\nRuntime: "
                            + audioEngine.watermarkStatus(),
                        "Recover via WavMark", "Cancel", this,
                        juce::ModalCallbackFunction::create(
                            [safe = juce::Component::SafePointer<ArrangementView>(this),
                             file = clip.mediaFile,
                             id = clip.id](int result)
                            {
                                if (safe == nullptr || result != 1) return;
                                IngredientInfo recoveredInfo;
                                const auto recovery = safe->audioEngine.recoverProvenance(
                                    file, recoveredInfo);
                                if (recovery.failed())
                                {
                                    safe->projectMessage = "Recovery failed: "
                                        + recovery.getErrorMessage();
                                    safe->refreshTransport();
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::MessageBoxIconType::WarningIcon,
                                        "WavMark Recovery", recovery.getErrorMessage(),
                                        "OK", safe.getComponent());
                                    return;
                                }
                                safe->recoveredProvenance[id.toStdString()] = recoveredInfo;
                                safe->projectMessage = "Recovered via WavMark soft binding";
                                safe->rebuildArrangement();
                                safe->showSelectedCredentials();
                            }));
                    return;
                }
                auto details = "File: " + clip.mediaFile.getFileName()
                    + "\nContent Credentials: " + (info.c2paPresent ? "Present" : "Not present")
                    + "\nStatus: " + provenanceStatusLabel(info.status);
                if (info.retrievalMode == ProvenanceRetrievalMode::recoveredSoftBinding)
                    details += "\nRetrieval: Recovered via WavMark soft binding"
                        "\nAlgorithm: " + info.softBindingAlgorithm
                        + "\nPayload: " + info.softBindingPayloadHex
                        + "\nHard binding: Not valid for this derivative";
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

void ArrangementView::importAudioFiles(const juce::Array<juce::File>& files, int x, int y)
{
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

    for (auto& track : snapshots)
        for (auto& clip : track.clips)
            if (const auto recovered = recoveredProvenance.find(clip.id.toStdString());
                recovered != recoveredProvenance.end())
                clip.provenance = recovered->second;

    for (std::size_t trackIndex = 0; trackIndex < snapshots.size(); ++trackIndex)
    {
        const auto colour = colourForTrack(static_cast<int>(trackIndex));
        auto header = std::make_unique<TrackHeaderView>(static_cast<int>(trackIndex));
        header->setState(snapshots[trackIndex].name, snapshots[trackIndex].gainDb,
                         snapshots[trackIndex].pan, snapshots[trackIndex].muted,
                         snapshots[trackIndex].soloed, colour);
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
        header->onPanChanged = [this](int index, double value)
        {
            deferTrackEdit([index, value](AudioEngine& engine)
                { return engine.setTrackPan(index, value); }, "Changed track pan");
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
        headerContainer.addAndMakeVisible(*header);
        trackHeaders.push_back(std::move(header));

        for (const auto& clip : snapshots[trackIndex].clips)
        {
            auto view = std::make_unique<WaveformView>(
                audioEngine.audioFormatManager(), audioEngine.audioThumbnailCache(),
                clip.mediaFile, clip.name, clip.id, static_cast<int>(trackIndex),
                clip.startSeconds, clip.sourceOffsetSeconds, clip.lengthSeconds, colour,
                clip.provenance.status, clip.provenance.retrievalMode);
            view->setSelected(clip.id == selectedClipId);
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
        audioEngine.transportSnapshot(), verticalOffset);
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
        zoomBy(wheel.deltaY > 0.0f ? 1.12 : 0.89, event.position.x);
        return;
    }
    if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        horizontalScroll.setCurrentRangeStart(geometry.scrollSeconds
            - (wheel.deltaX + wheel.deltaY) * 3.0);
    else
        verticalScroll.setCurrentRangeStart(verticalOffset - wheel.deltaY * trackHeight * 2.0);
}

void ArrangementView::selectClip(const juce::String& id)
{
    selectedClipId = id;
    for (auto& view : waveformViews)
        view->setSelected(view->id() == selectedClipId);
    grabKeyboardFocus();
}

void ArrangementView::handleClipGesture(WaveformView& view, WaveformView::DragMode mode,
                                        int deltaX, int deltaY, bool finished, bool bypassSnap)
{
    if (! finished)
    {
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
