#pragma once

#include "TrackLevelMeter.h"

#include <tracktion_engine/tracktion_engine.h>

#include <functional>
#include <memory>
#include <vector>

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
    double loopStartSeconds = 0.0;
    double loopEndSeconds = 0.0;
};

struct AudioFileMetadata
{
    double lengthSeconds = 0.0;
    double sampleRate = 0.0;
    int channels = 0;
};

struct MidiPlaybackNote
{
    int noteNumber = 60;
    double startBeats = 0.0;
    double durationBeats = 1.0;
    int velocity = 100;
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
    [[nodiscard]] TrackLevelSnapshot trackLevelSnapshot(int trackIndex) noexcept;

    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop);
    void setLoopRange(double startSeconds, double endSeconds);
    void setBpm(double bpm);
    [[nodiscard]] juce::Result inspectAudioFile(const juce::File& file,
                                                AudioFileMetadata& metadata);
    [[nodiscard]] juce::Result insertAudioClip(const juce::File& file,
                                               const juce::String& name,
                                               int trackIndex,
                                               double startSeconds,
                                               double sourceOffsetSeconds,
                                               double lengthSeconds);
    [[nodiscard]] juce::Result insertMidiClip(const juce::String& name,
                                              int trackIndex,
                                              double startBeats,
                                              double lengthBeats,
                                              const std::vector<MidiPlaybackNote>& notes);
    [[nodiscard]] juce::Result setTrackProperties(int trackIndex,
                                                   const juce::String& name,
                                                   double gainDb,
                                                   double pan,
                                                   bool muted,
                                                   bool soloed);
    [[nodiscard]] juce::Result setTrackMute(int trackIndex, bool muted);
    [[nodiscard]] juce::Result setTrackSolo(int trackIndex, bool soloed);
    [[nodiscard]] juce::Result setTrackGain(int trackIndex, double gainDb);
    [[nodiscard]] juce::Result setTrackPan(int trackIndex, double pan);
    [[nodiscard]] juce::Result sendLiveMidiMessage(int trackIndex,
                                                   const juce::MidiMessage&);
    void allNotesOff(int trackIndex);
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
    [[nodiscard]] juce::Result renderTrackWav(const juce::File&, int trackIndex,
                                              double endSeconds, double startSeconds = 0.0,
                                              bool includeTrackMix = true);
    // Owner-thread scope for a worker render. Do not persist render-only audibility.
    [[nodiscard]] juce::Result beginExclusiveTrackRender(int trackIndex = -1,
                                                         bool includeTrackMix = true);
    void finishExclusiveTrackRender();
    // Owner-thread, export-only substitutions; never save while this scope is active.
    [[nodiscard]] juce::Result beginOfflineExport();
    [[nodiscard]] juce::Result substituteMidiExportStem(int trackIndex, const juce::File&,
                                                        double startSeconds, double lengthSeconds);
    void finishOfflineExport();
    [[nodiscard]] juce::AudioFormatManager& audioFormatManager() noexcept;
    [[nodiscard]] juce::AudioThumbnailCache& audioThumbnailCache() noexcept;
    void setBeforeEditReplacement(std::function<void()> callback);
    [[nodiscard]] bool createProjectEdit(const juce::File& editFile);
    [[nodiscard]] bool saveProjectEdit(const juce::File& editFile);
    [[nodiscard]] bool loadProjectEdit(const juce::File& editFile);
    void closeProjectEdit();

private:
    [[nodiscard]] juce::Result renderWavInternal(const juce::File&, double endSeconds,
                                                 int onlyTrackIndex, double startSeconds = 0.0);
    void ensureTrackLevelMeter(tracktion::engine::AudioTrack&);
    void configurePreferredAudioSettings();
    void configureLoadedAudioClips();
    void prepareEdit();

    tracktion::engine::Engine engine;
    std::unique_ptr<tracktion::engine::Edit> edit;
    std::function<void()> beforeEditReplacement;
    bool initialised = false;
    std::vector<std::pair<bool, bool>> renderAudibility;
    double renderOriginalPosition = 0.0;
    bool exclusiveTrackRender = false;
    tracktion::engine::VolumeAndPanPlugin::Ptr renderVolumePlugin;
    bool renderVolumeWasEnabled = false;
    struct MidiExportReplacement
    {
        tracktion::engine::Clip::Ptr audio;
        std::vector<std::pair<tracktion::engine::Clip::Ptr, bool>> midi;
        tracktion::engine::Plugin::Ptr instrument;
        bool instrumentEnabled = false;
    };
    std::vector<MidiExportReplacement> midiExportReplacements;
    bool offlineExportActive = false;
    double offlineExportPosition = 0.0;
    std::unique_ptr<tracktion::engine::TransportControl::ReallocationInhibitor> offlineExportInhibitor;
};
}
