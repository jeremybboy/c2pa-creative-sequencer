#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace c2paseq
{
class AudioEngine;
class WaveformView;

class ArrangementView final : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              private juce::Timer
{
public:
    explicit ArrangementView(AudioEngine& audioEngine);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void refreshTransport();
    void showAudioSettings();
    void createProject();
    void openProject();
    void saveProject();
    void chooseAudioFiles();
    void importAudioFiles(const juce::Array<juce::File>& files);
    void rebuildArrangement();
    void showProjectResult(const juce::Result& result, const juce::String& successMessage);

    AudioEngine& audioEngine;
    juce::Label title;
    juce::Label emptyState;
    juce::Label status;
    juce::Label projectName;
    juce::TextButton newProject { "New" };
    juce::TextButton openProjectButton { "Open" };
    juce::TextButton saveProjectButton { "Save" };
    juce::TextButton importAudioButton { "Import Audio" };
    juce::TextButton audioSettings { "Audio Device" };
    juce::TextButton playPause { "Play" };
    juce::TextButton stop { "Stop" };
    juce::TextButton loop { "Loop" };
    juce::Label position;
    juce::Slider bpm;
    juce::Slider scrubber;
    bool scrubberIsDragging = false;
    juce::String projectMessage;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<std::unique_ptr<WaveformView>> waveformViews;
};
}
