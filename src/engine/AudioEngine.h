#pragma once

#include "ProjectEngine.h"
#include "TracktionAdapter.h"
#include "export/ExportResult.h"
#include "provenance/ProvenanceService.h"
#include "plugins/PluginHost.h"

#include <memory>

namespace c2paseq
{
class AudioEngine final
{
public:
    explicit AudioEngine(std::unique_ptr<SigningProvider> signingProvider = {},
                         juce::File pluginCacheFile = {});

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String status() const;
    [[nodiscard]] AudioDeviceSnapshot audioDeviceSnapshot() const;
    [[nodiscard]] TransportSnapshot transportSnapshot() const;
    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop, const juce::String& selectedClipId = {});
    void setBpm(double bpm);
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           int trackIndex,
                                           double startSeconds);
    [[nodiscard]] juce::Result moveClip(const juce::String&, int trackIndex, double startSeconds);
    [[nodiscard]] juce::Result trimClip(const juce::String&, double startSeconds,
                                        double sourceOffsetSeconds, double lengthSeconds);
    [[nodiscard]] juce::Result deleteClip(const juce::String&);
    [[nodiscard]] juce::Result duplicateClip(const juce::String&);
    [[nodiscard]] juce::Result splitClip(const juce::String&, double positionSeconds);
    [[nodiscard]] juce::Result setTrackName(int, const juce::String&);
    [[nodiscard]] juce::Result setTrackMute(int, bool);
    [[nodiscard]] juce::Result setTrackSolo(int, bool);
    [[nodiscard]] juce::Result setTrackGain(int, double);
    [[nodiscard]] juce::Result setTrackPan(int, double);
    [[nodiscard]] const std::vector<PluginDescriptor>& availableVst3Plugins() const noexcept;
    [[nodiscard]] juce::Result scanVst3Plugins(const juce::FileSearchPath& paths = {});
    [[nodiscard]] juce::Result loadTrackPlugin(int, const juce::String& identifier);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int, bool);
    [[nodiscard]] juce::Result removeTrackPlugin(int);
    [[nodiscard]] juce::Result openTrackPluginEditor(int);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    void setTimelineView(double pixelsPerSecond, double scrollSeconds);
    [[nodiscard]] double timelinePixelsPerSecond() const noexcept;
    [[nodiscard]] double timelineScrollSeconds() const noexcept;
    [[nodiscard]] static bool isSupportedAudioFile(const juce::File& file);
    [[nodiscard]] std::vector<ArrangementTrackSnapshot> arrangementSnapshot() const;
    [[nodiscard]] juce::AudioFormatManager& audioFormatManager() noexcept;
    [[nodiscard]] juce::AudioThumbnailCache& audioThumbnailCache() noexcept;
    [[nodiscard]] IngredientInfo inspectProvenance(const juce::File&) const;
    [[nodiscard]] bool signingConfigured() const;
    [[nodiscard]] juce::String signingCredentialStatus() const;
    [[nodiscard]] juce::Result configureSigningCredential(const juce::File&);
    [[nodiscard]] juce::Result removeSigningCredential();
    [[nodiscard]] ExportResult exportMix(const juce::File& destination);

    [[nodiscard]] juce::Result createProject(const juce::File& projectFolder,
                                             const juce::String& projectName);
    [[nodiscard]] juce::Result saveProject();
    [[nodiscard]] juce::Result openProject(const juce::File& projectFolder);
    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String projectName() const;

private:
    TracktionAdapter tracktion;
    ProvenanceService provenance;
    ProjectEngine projectEngine;
    PluginHost pluginHost;
};
}
