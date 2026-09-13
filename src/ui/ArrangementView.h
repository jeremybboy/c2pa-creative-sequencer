#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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

    AudioEngine& audioEngine;
    juce::Label title;
    juce::Label emptyState;
    juce::Label status;
    juce::TextButton audioSettings { "Audio Device" };
    juce::TextButton playPause { "Play" };
    juce::TextButton stop { "Stop" };
    juce::TextButton loop { "Loop" };
    juce::Label position;
    juce::Slider bpm;
    juce::Slider scrubber;
    bool scrubberIsDragging = false;
};
}
