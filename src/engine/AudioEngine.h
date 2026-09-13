#pragma once

#include "ProjectEngine.h"
#include "TracktionAdapter.h"

namespace c2paseq
{
class AudioEngine final
{
public:
    AudioEngine();

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String status() const;
    [[nodiscard]] AudioDeviceSnapshot audioDeviceSnapshot() const;
    [[nodiscard]] TransportSnapshot transportSnapshot() const;
    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop);
    void setBpm(double bpm);
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           double startSeconds);
    [[nodiscard]] static bool isSupportedAudioFile(const juce::File& file);
    [[nodiscard]] std::vector<ArrangementTrackSnapshot> arrangementSnapshot() const;
    [[nodiscard]] juce::AudioFormatManager& audioFormatManager() noexcept;
    [[nodiscard]] juce::AudioThumbnailCache& audioThumbnailCache() noexcept;

    [[nodiscard]] juce::Result createProject(const juce::File& projectFolder,
                                             const juce::String& projectName);
    [[nodiscard]] juce::Result saveProject();
    [[nodiscard]] juce::Result openProject(const juce::File& projectFolder);
    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String projectName() const;

private:
    TracktionAdapter tracktion;
    ProjectEngine projectEngine;
};
}
