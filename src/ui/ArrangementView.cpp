#include "ArrangementView.h"

#include "engine/AudioEngine.h"
#include "transport/TransportFormatting.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace c2paseq
{
ArrangementView::ArrangementView(AudioEngine& engine)
    : audioEngine(engine)
{
    title.setText("ARRANGEMENT", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour::fromRGB(242, 193, 78));

    emptyState.setText("Empty arrangement\nAudio-stem import arrives in PR 004", juce::dontSendNotification);
    emptyState.setFont(juce::FontOptions(24.0f));
    emptyState.setJustificationType(juce::Justification::centred);
    emptyState.setColour(juce::Label::textColourId, juce::Colour::fromRGB(224, 218, 207));

    status.setFont(juce::FontOptions(13.0f));
    status.setColour(juce::Label::textColourId, juce::Colour::fromRGB(152, 171, 168));

    projectName.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    projectName.setColour(juce::Label::textColourId, juce::Colour::fromRGB(224, 218, 207));
    projectName.setJustificationType(juce::Justification::centredLeft);

    newProject.onClick = [this] { createProject(); };
    openProjectButton.onClick = [this] { openProject(); };
    saveProjectButton.onClick = [this] { saveProject(); };

    playPause.onClick = [this]
    {
        if (audioEngine.transportSnapshot().playing)
            audioEngine.pause();
        else
            audioEngine.play();

        refreshTransport();
    };

    stop.onClick = [this]
    {
        audioEngine.stop();
        refreshTransport();
    };

    loop.setClickingTogglesState(true);
    loop.onClick = [this]
    {
        audioEngine.setLooping(loop.getToggleState());
        refreshTransport();
    };

    audioSettings.onClick = [this] { showAudioSettings(); };

    position.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    position.setJustificationType(juce::Justification::centred);
    position.setColour(juce::Label::textColourId, juce::Colour::fromRGB(224, 218, 207));

    bpm.setSliderStyle(juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle(juce::Slider::TextBoxRight, false, 82, 26);
    bpm.setRange(transport::minimumBpm, transport::maximumBpm, 0.1);
    bpm.setValue(transport::defaultBpm, juce::dontSendNotification);
    bpm.setTextValueSuffix(" BPM");
    bpm.onValueChange = [this] { audioEngine.setBpm(bpm.getValue()); };

    scrubber.setSliderStyle(juce::Slider::LinearHorizontal);
    scrubber.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    scrubber.setRange(0.0, transport::timelineEndSeconds, 0.001);
    scrubber.onDragStart = [this] { scrubberIsDragging = true; };
    scrubber.onValueChange = [this]
    {
        if (scrubberIsDragging)
            audioEngine.seek(scrubber.getValue());
    };
    scrubber.onDragEnd = [this]
    {
        audioEngine.seek(scrubber.getValue());
        scrubberIsDragging = false;
    };

    addAndMakeVisible(title);
    addAndMakeVisible(emptyState);
    addAndMakeVisible(status);
    addAndMakeVisible(projectName);
    addAndMakeVisible(newProject);
    addAndMakeVisible(openProjectButton);
    addAndMakeVisible(saveProjectButton);
    addAndMakeVisible(audioSettings);
    addAndMakeVisible(playPause);
    addAndMakeVisible(stop);
    addAndMakeVisible(loop);
    addAndMakeVisible(position);
    addAndMakeVisible(bpm);
    addAndMakeVisible(scrubber);

    refreshTransport();
    startTimerHz(30);
}

void ArrangementView::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour::fromRGB(25, 25, 24));
    graphics.setColour(juce::Colour::fromRGB(67, 65, 61));

    const auto content = getLocalBounds().reduced(24).withTrimmedTop(44).withTrimmedBottom(36);
    constexpr int rowHeight = 92;

    for (int y = content.getY(); y <= content.getBottom(); y += rowHeight)
        graphics.drawHorizontalLine(y, static_cast<float>(content.getX()), static_cast<float>(content.getRight()));

    graphics.setColour(juce::Colour::fromRGB(212, 75, 55));
    graphics.fillRect(content.getX(), content.getY(), 3, content.getHeight());
}

void ArrangementView::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    title.setBounds(bounds.removeFromTop(32));
    auto transportBar = bounds.removeFromTop(44);

    audioSettings.setBounds(transportBar.removeFromRight(126).reduced(3));
    bpm.setBounds(transportBar.removeFromRight(180).reduced(3));
    position.setBounds(transportBar.removeFromRight(112).reduced(3));
    loop.setBounds(transportBar.removeFromLeft(72).reduced(3));
    stop.setBounds(transportBar.removeFromLeft(72).reduced(3));
    playPause.setBounds(transportBar.removeFromLeft(82).reduced(3));

    auto projectBar = bounds.removeFromTop(34);
    newProject.setBounds(projectBar.removeFromLeft(70).reduced(3));
    openProjectButton.setBounds(projectBar.removeFromLeft(70).reduced(3));
    saveProjectButton.setBounds(projectBar.removeFromLeft(70).reduced(3));
    projectName.setBounds(projectBar.reduced(6, 2));

    scrubber.setBounds(bounds.removeFromTop(24));
    status.setBounds(bounds.removeFromBottom(28));
    emptyState.setBounds(bounds);
}

void ArrangementView::timerCallback()
{
    refreshTransport();
}

void ArrangementView::refreshTransport()
{
    const auto snapshot = audioEngine.transportSnapshot();
    playPause.setButtonText(snapshot.playing ? "Pause" : "Play");
    loop.setToggleState(snapshot.looping, juce::dontSendNotification);
    position.setText(transport::formatPosition(snapshot.positionSeconds).c_str(), juce::dontSendNotification);
    bpm.setValue(snapshot.bpm, juce::dontSendNotification);
    projectName.setText("Project: " + audioEngine.projectName(), juce::dontSendNotification);
    saveProjectButton.setEnabled(audioEngine.hasProject());
    const auto message = projectMessage.isNotEmpty() ? projectMessage + " | " : juce::String();
    status.setText(message + audioEngine.status(), juce::dontSendNotification);

    if (! scrubberIsDragging)
        scrubber.setValue(snapshot.positionSeconds, juce::dontSendNotification);
}

void ArrangementView::createProject()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Create Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Untitled Project.c2paseq"),
        "*.c2paseq");

    const auto flags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;
    fileChooser->launchAsync(flags, [safe = juce::Component::SafePointer<ArrangementView>(this)]
    (const juce::FileChooser& chooser)
    {
        if (safe == nullptr || chooser.getResult() == juce::File())
            return;
        auto folder = chooser.getResult();
        if (! folder.hasFileExtension("c2paseq"))
            folder = folder.withFileExtension("c2paseq");
        safe->showProjectResult(
            safe->audioEngine.createProject(folder, folder.getFileNameWithoutExtension()),
            "Created " + folder.getFileNameWithoutExtension());
        safe->fileChooser.reset();
    });
}

void ArrangementView::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Open Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.c2paseq");

    const auto flags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectDirectories
        | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [safe = juce::Component::SafePointer<ArrangementView>(this)]
    (const juce::FileChooser& chooser)
    {
        if (safe == nullptr || chooser.getResult() == juce::File())
            return;
        const auto folder = chooser.getResult();
        safe->showProjectResult(safe->audioEngine.openProject(folder),
                                "Opened " + folder.getFileNameWithoutExtension());
        safe->fileChooser.reset();
    });
}

void ArrangementView::saveProject()
{
    showProjectResult(audioEngine.saveProject(), "Project saved");
}

void ArrangementView::showProjectResult(const juce::Result& result,
                                         const juce::String& successMessage)
{
    projectMessage = result.wasOk() ? successMessage : "Project error: " + result.getErrorMessage();
    refreshTransport();
}

void ArrangementView::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(
        audioEngine.audioDeviceManager(), 0, 0, 1, 2, false, false, true, false);
    selector->setSize(520, 360);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio Device";
    options.dialogBackgroundColour = juce::Colour::fromRGB(30, 30, 29);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}
}
