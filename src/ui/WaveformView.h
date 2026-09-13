#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace c2paseq
{
class WaveformView final : public juce::Component,
                           private juce::ChangeListener
{
public:
    WaveformView(juce::AudioFormatManager& formatManager,
                 juce::AudioThumbnailCache& thumbnailCache,
                 juce::File audioFile,
                 juce::String clipName);

    void paint(juce::Graphics& graphics) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    juce::AudioThumbnail thumbnail;
    juce::File file;
    juce::String name;
};
}
