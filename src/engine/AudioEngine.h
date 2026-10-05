#pragma once

#include "ProjectEngine.h"
#include "TracktionAdapter.h"
#include "export/ExportResult.h"
#include "provenance/ProvenanceService.h"
#include "plugins/PluginHost.h"
#include "fingerprint/AudfprintService.h"
#include "watermark/AudioWMarkService.h"
#include "watermark/SoftBindingOutbox.h"

#include <memory>

namespace c2paseq
{
class AudioEngine final
{
public:
    explicit AudioEngine(std::unique_ptr<SigningProvider> signingProvider = {},
                         juce::File pluginCacheFile = {},
                         bool showPluginWindows = true,
                         std::unique_ptr<WatermarkService> watermarkService = {},
                         juce::File softBindingOutboxDirectory = {},
                         std::unique_ptr<FingerprintService> fingerprintService = {});

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String status() const;
    [[nodiscard]] AudioDeviceSnapshot audioDeviceSnapshot() const;
    [[nodiscard]] TransportSnapshot transportSnapshot() const;
    [[nodiscard]] TrackLevelSnapshot trackLevelSnapshot(int trackIndex) noexcept;
    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop, const juce::String& selectedClipId = {});
    [[nodiscard]] juce::Result setLoopRangeAndEnable(double startSeconds,
                                                     double endSeconds);
    void setBpm(double bpm);
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           int trackIndex,
                                           double startSeconds);
    [[nodiscard]] juce::Result createMidiClip(int trackIndex, double startBeats,
                                              double lengthBeats = 16.0);
    [[nodiscard]] juce::Result moveMidiClip(const juce::String&, int trackIndex,
                                            double startBeats);
    [[nodiscard]] juce::Result trimMidiClip(const juce::String&, double startBeats,
                                            double lengthBeats);
    [[nodiscard]] juce::Result addMidiNote(const juce::String& clipId, int noteNumber,
                                           double startBeats, double durationBeats,
                                           int velocity);
    [[nodiscard]] juce::Result updateMidiNote(const juce::String& clipId,
                                              const juce::String& noteId,
                                              int noteNumber, double startBeats,
                                              double durationBeats, int velocity);
    [[nodiscard]] juce::Result deleteMidiNote(const juce::String& clipId,
                                              const juce::String& noteId);
    [[nodiscard]] juce::Result insertMidiNotes(
        const juce::String& clipId,
        const std::vector<ArrangementMidiNoteSnapshot>& notes);
    [[nodiscard]] juce::Result deleteMidiNotes(
        const juce::String& clipId,
        const std::vector<juce::String>& noteIds);
    [[nodiscard]] juce::Result moveClip(const juce::String&, int trackIndex, double startSeconds);
    [[nodiscard]] juce::Result trimClip(const juce::String&, double startSeconds,
                                        double sourceOffsetSeconds, double lengthSeconds);
    [[nodiscard]] juce::Result deleteClip(const juce::String&);
    [[nodiscard]] juce::Result deleteClips(const std::vector<juce::String>&);
    [[nodiscard]] juce::Result duplicateClip(const juce::String&);
    [[nodiscard]] juce::Result copyClips(const std::vector<juce::String>&);
    [[nodiscard]] juce::Result cutClips(const std::vector<juce::String>&);
    [[nodiscard]] juce::Result duplicateClips(const std::vector<juce::String>&);
    [[nodiscard]] juce::Result copyTimeRange(const ArrangementTimeSelection&);
    [[nodiscard]] juce::Result cutTimeRange(const ArrangementTimeSelection&);
    [[nodiscard]] juce::Result duplicateTimeRange(const ArrangementTimeSelection&);
    [[nodiscard]] juce::Result pasteClipboard(double destinationSeconds,
                                              int destinationTrack = -1);
    [[nodiscard]] bool hasClipboard() const noexcept;
    [[nodiscard]] juce::Result splitClip(const juce::String&, double positionSeconds);
    [[nodiscard]] juce::Result addAudioTrack();
    [[nodiscard]] juce::Result addMidiTrack();
    [[nodiscard]] juce::Result deleteTrack(int);
    [[nodiscard]] juce::Result deleteAudioTrack(int);
    [[nodiscard]] juce::Result setTrackName(int, const juce::String&);
    [[nodiscard]] juce::Result setTrackMute(int, bool);
    [[nodiscard]] juce::Result setTrackSolo(int, bool);
    [[nodiscard]] juce::Result setTrackGain(int, double);
    [[nodiscard]] juce::Result setTrackPan(int, double);
    [[nodiscard]] juce::Result beginTrackMixGesture(int);
    [[nodiscard]] juce::Result previewTrackGain(int, double);
    [[nodiscard]] juce::Result previewTrackPan(int, double);
    [[nodiscard]] juce::Result endTrackMixGesture(int);
    [[nodiscard]] juce::Result sendLiveMidiMessage(int trackIndex,
                                                   const juce::MidiMessage&);
    void allNotesOff(int trackIndex);
    [[nodiscard]] const std::vector<PluginDescriptor>& availableVst3Plugins() const noexcept;
    [[nodiscard]] juce::Result scanVst3Plugins(const juce::FileSearchPath& paths = {});
    [[nodiscard]] juce::Result scanVst3PluginBundle(const juce::File& bundle);
    [[nodiscard]] juce::Result loadTrackPlugin(int, const juce::String& identifier);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int, bool);
    [[nodiscard]] juce::Result removeTrackPlugin(int);
    [[nodiscard]] juce::Result openTrackPluginEditor(
        int, std::function<bool(const juce::KeyPress&)> keyHandler = {});
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
    void setSoftBindingEnabled(bool enabled) noexcept;
    [[nodiscard]] bool softBindingEnabled() const noexcept;
    [[nodiscard]] bool watermarkAvailable() const;
    [[nodiscard]] juce::String watermarkStatus() const;
    void setFingerprintEnabled(bool enabled) noexcept;
    [[nodiscard]] bool fingerprintEnabled() const noexcept;
    [[nodiscard]] juce::String fingerprintStatus() const;
    [[nodiscard]] ExportResult exportMix(
        const juce::File& destination,
        ExportProgressCallback progress = {},
        ExportCancellationCheck shouldCancel = {});

    [[nodiscard]] juce::Result createProject(const juce::File& projectFolder,
                                             const juce::String& projectName);
    [[nodiscard]] juce::Result saveProject();
    [[nodiscard]] juce::Result openProject(const juce::File& projectFolder);
    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String projectName() const;

private:
    TracktionAdapter tracktion;
    ProvenanceService provenance;
    std::unique_ptr<WatermarkService> watermark;
    std::unique_ptr<FingerprintService> fingerprint;
    SoftBindingOutbox softBindingOutbox;
    ProjectEngine projectEngine;
    PluginHost pluginHost;
    bool useSoftBinding = false;
    bool useFingerprint = false;
};
}
