#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

namespace c2paseq
{
class SampleAuditionPlayer final
{
public:
    SampleAuditionPlayer(juce::AudioDeviceManager&, juce::AudioFormatManager&);
    ~SampleAuditionPlayer();

    [[nodiscard]] juce::Result preview(const juce::File&);
    void stop();
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const juce::File& currentFile() const noexcept;

private:
    juce::AudioDeviceManager& deviceManager;
    juce::AudioFormatManager& formatManager;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer output;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::File file;
};
}
