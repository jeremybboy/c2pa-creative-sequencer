#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <memory>

namespace c2paseq
{
struct AudioDeviceSnapshot
{
    juce::String name;
    double sampleRate = 0.0;
    int blockSize = 0;
    bool open = false;
};

struct TransportSnapshot
{
    double positionSeconds = 0.0;
    double bpm = 120.0;
    bool playing = false;
    bool looping = false;
};

struct AudioFileMetadata
{
    double lengthSeconds = 0.0;
    double sampleRate = 0.0;
    int channels = 0;
};

class TracktionAdapter final
{
public:
    TracktionAdapter();
    ~TracktionAdapter();

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String audioDeviceDescription() const;
    [[nodiscard]] AudioDeviceSnapshot audioDeviceSnapshot() const;
    [[nodiscard]] TransportSnapshot transportSnapshot() const;

    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop);
    void setBpm(double bpm);
    [[nodiscard]] juce::Result inspectAudioFile(const juce::File& file,
                                                AudioFileMetadata& metadata);
    [[nodiscard]] juce::Result insertAudioClip(const juce::File& file,
                                               const juce::String& name,
                                               int trackIndex,
                                               double startSeconds,
                                               double sourceOffsetSeconds,
                                               double lengthSeconds);
    [[nodiscard]] juce::Result setTrackProperties(int trackIndex,
                                                   const juce::String& name,
                                                   double gainDb,
                                                   double pan,
                                                   bool muted,
                                                   bool soloed);
    [[nodiscard]] juce::Result setTrackMute(int trackIndex, bool muted);
    [[nodiscard]] juce::Result setTrackSolo(int trackIndex, bool soloed);
    void registerPluginDescription(const juce::PluginDescription&);
    [[nodiscard]] juce::Result setTrackPlugin(int trackIndex,
                                               const juce::PluginDescription&,
                                               const juce::String& stateBase64,
                                               bool bypassed);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int trackIndex, bool bypassed);
    [[nodiscard]] juce::Result removeTrackPlugin(int trackIndex);
    [[nodiscard]] juce::Result captureTrackPluginState(int trackIndex,
                                                       juce::String& stateBase64,
                                                       bool& bypassed,
                                                       bool& missing);
    [[nodiscard]] juce::AudioPluginInstance* trackPluginInstance(int trackIndex) const;
    [[nodiscard]] juce::Result renderWav(const juce::File& destination,
                                          double endSeconds);
    [[nodiscard]] juce::AudioFormatManager& audioFormatManager() noexcept;
    [[nodiscard]] juce::AudioThumbnailCache& audioThumbnailCache() noexcept;
    [[nodiscard]] bool createProjectEdit(const juce::File& editFile);
    [[nodiscard]] bool saveProjectEdit(const juce::File& editFile);
    [[nodiscard]] bool loadProjectEdit(const juce::File& editFile);
    void closeProjectEdit();

private:
    void configurePreferredAudioSettings();
    void configureLoadedAudioClips();
    void prepareEdit();

    tracktion::engine::Engine engine;
    std::unique_ptr<tracktion::engine::Edit> edit;
    bool initialised = false;
};
}
