#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace c2paseq
{
class AudioEngine;

class ArrangementView final : public juce::Component,
                              private juce::Timer
{
public:
    explicit ArrangementView(AudioEngine& audioEngine);

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshTransport();
    void showAudioSettings();
    void createProject();
    void openProject();
    void saveProject();
    void showProjectResult(const juce::Result& result, const juce::String& successMessage);

    AudioEngine& audioEngine;
    juce::Label title;
    juce::Label emptyState;
    juce::Label status;
    juce::Label projectName;
    juce::TextButton newProject { "New" };
    juce::TextButton openProjectButton { "Open" };
    juce::TextButton saveProjectButton { "Save" };
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
};
}
