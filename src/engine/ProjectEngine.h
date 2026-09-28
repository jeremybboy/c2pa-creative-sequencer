#pragma once

#include "project/Project.h"
#include "project/ProjectPaths.h"
#include "timeline/ArrangementSelection.h"

#include <optional>
#include <functional>
#include <vector>

namespace c2paseq
{
class TracktionAdapter;
class ProvenanceService;
struct PluginDescriptor;

struct TrackPluginSnapshot
{
    juce::String identifier;
    juce::String name;
    juce::String vendor;
    bool bypassed = false;
    bool missing = false;
};

struct ArrangementClipSnapshot
{
    juce::String id;
    juce::String mediaId;
    juce::String name;
    juce::File mediaFile;
    double startSeconds = 0.0;
    double sourceOffsetSeconds = 0.0;
    double lengthSeconds = 0.0;
    IngredientInfo provenance;
};

struct ArrangementMidiNoteSnapshot
{
    juce::String id;
    int noteNumber = 60;
    double startBeats = 0.0;
    double durationBeats = 1.0;
    int velocity = 100;
};

struct ArrangementMidiClipSnapshot
{
    juce::String id;
    double startBeats = 0.0;
    double lengthBeats = 4.0;
    std::vector<ArrangementMidiNoteSnapshot> notes;
};

struct ArrangementTrackSnapshot
{
    juce::String id;
    juce::String name;
    TrackType type = TrackType::audio;
    double gainDb = 0.0;
    double pan = 0.0;
    bool muted = false;
    bool soloed = false;
    std::optional<TrackPluginSnapshot> plugin;
    std::vector<ArrangementClipSnapshot> clips;
    std::vector<ArrangementMidiClipSnapshot> midiClips;
    std::size_t midiClipCount = 0;
};

class ProjectEngine final
{
public:
    ProjectEngine(TracktionAdapter& tracktionAdapter, ProvenanceService& provenanceService);

    [[nodiscard]] juce::Result createProject(const juce::File& projectFolder,
                                             const juce::String& projectName);
    [[nodiscard]] juce::Result saveProject();
    [[nodiscard]] juce::Result openProject(const juce::File& projectFolder);
    void closeProject();
    void setBpm(double bpm);
    void setLooping(bool shouldLoop, const juce::String& selectedClipId);
    [[nodiscard]] juce::Result setLoopRangeAndEnable(double startSeconds,
                                                     double endSeconds);
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           int trackIndex,
                                           double startSeconds);
    [[nodiscard]] juce::Result createMidiClip(int trackIndex,
                                              double startBeats,
                                              double lengthBeats = 16.0);
    [[nodiscard]] juce::Result moveMidiClip(const juce::String& clipId,
                                            int trackIndex,
                                            double startBeats);
    [[nodiscard]] juce::Result trimMidiClip(const juce::String& clipId,
                                            double startBeats,
                                            double lengthBeats);
    [[nodiscard]] juce::Result addMidiNote(const juce::String& clipId,
                                           int noteNumber,
                                           double startBeats,
                                           double durationBeats,
                                           int velocity);
    [[nodiscard]] juce::Result updateMidiNote(const juce::String& clipId,
                                              const juce::String& noteId,
                                              int noteNumber,
                                              double startBeats,
                                              double durationBeats,
                                              int velocity);
    [[nodiscard]] juce::Result deleteMidiNote(const juce::String& clipId,
                                              const juce::String& noteId);
    [[nodiscard]] juce::Result insertMidiNotes(
        const juce::String& clipId,
        const std::vector<ArrangementMidiNoteSnapshot>& notes);
    [[nodiscard]] juce::Result deleteMidiNotes(
        const juce::String& clipId,
        const std::vector<juce::String>& noteIds);
    [[nodiscard]] juce::Result moveClip(const juce::String& clipId,
                                        int trackIndex,
                                        double startSeconds);
    [[nodiscard]] juce::Result trimClip(const juce::String& clipId,
                                        double startSeconds,
                                        double sourceOffsetSeconds,
                                        double lengthSeconds);
    [[nodiscard]] juce::Result deleteClip(const juce::String& clipId);
    [[nodiscard]] juce::Result deleteClips(const std::vector<juce::String>& clipIds);
    [[nodiscard]] juce::Result duplicateClip(const juce::String& clipId);
    [[nodiscard]] juce::Result copyClips(const std::vector<juce::String>& clipIds);
    [[nodiscard]] juce::Result cutClips(const std::vector<juce::String>& clipIds);
    [[nodiscard]] juce::Result duplicateClips(const std::vector<juce::String>& clipIds);
    [[nodiscard]] juce::Result copyTimeRange(const ArrangementTimeSelection& selection);
    [[nodiscard]] juce::Result cutTimeRange(const ArrangementTimeSelection& selection);
    [[nodiscard]] juce::Result duplicateTimeRange(const ArrangementTimeSelection& selection);
    [[nodiscard]] juce::Result pasteClipboard(double destinationSeconds,
                                              int destinationTrack = -1);
    [[nodiscard]] bool hasClipboard() const noexcept;
    [[nodiscard]] juce::Result splitClip(const juce::String& clipId,
                                         double positionSeconds);
    [[nodiscard]] juce::Result addAudioTrack();
    [[nodiscard]] juce::Result addMidiTrack();
    [[nodiscard]] juce::Result deleteTrack(int trackIndex);
    [[nodiscard]] juce::Result deleteAudioTrack(int trackIndex);
    [[nodiscard]] juce::Result setTrackName(int trackIndex, const juce::String& name);
    [[nodiscard]] juce::Result setTrackMute(int trackIndex, bool muted);
    [[nodiscard]] juce::Result setTrackSolo(int trackIndex, bool soloed);
    [[nodiscard]] juce::Result setTrackGain(int trackIndex, double gainDb);
    [[nodiscard]] juce::Result setTrackPan(int trackIndex, double pan);
    [[nodiscard]] juce::Result beginTrackMixGesture(int trackIndex);
    [[nodiscard]] juce::Result previewTrackGain(int trackIndex, double gainDb);
    [[nodiscard]] juce::Result previewTrackPan(int trackIndex, double pan);
    [[nodiscard]] juce::Result endTrackMixGesture(int trackIndex);
    [[nodiscard]] juce::Result setTrackPlugin(int trackIndex,
                                               const PluginDescriptor&);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int trackIndex, bool bypassed);
    [[nodiscard]] juce::Result removeTrackPlugin(int trackIndex);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    void setTimelineView(double pixelsPerSecond, double scrollSeconds);

    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String displayName() const;
    [[nodiscard]] const Project* currentProject() const noexcept;
    [[nodiscard]] const ProjectPaths* currentPaths() const noexcept;
    [[nodiscard]] std::vector<ArrangementTrackSnapshot> arrangementSnapshot() const;

private:
    [[nodiscard]] juce::Result rebuildEditFromProject();
    [[nodiscard]] juce::Result commitMutation(Project previous);
    [[nodiscard]] juce::Result mutateProject(
        const std::function<juce::Result(Project&)>& mutation);
    [[nodiscard]] juce::Result commitLiveTrackAudibility(Project previous,
                                                         int trackIndex,
                                                         bool solo);
    void cancelTrackMixGesture();
    [[nodiscard]] PluginState* pluginForTrack(int trackIndex);
    [[nodiscard]] const PluginState* pluginForTrack(int trackIndex) const;
    void ensureTrackCount(int count);

    struct ClipboardClip
    {
        ClipModel clip;
        int relativeTrack = 0;
        double relativeStartSeconds = 0.0;
    };

    struct ClipboardMidiClip
    {
        MidiClipModel clip;
        int relativeTrack = 0;
        double relativeStartSeconds = 0.0;
    };

    [[nodiscard]] juce::Result fillClipboardFromClips(
        const std::vector<juce::String>& clipIds);
    [[nodiscard]] juce::Result fillClipboardFromTimeRange(
        const ArrangementTimeSelection& selection);

    TracktionAdapter& tracktion;
    ProvenanceService& provenance;
    std::optional<Project> project;
    std::optional<ProjectPaths> paths;
    std::vector<Project> undoHistory;
    std::vector<Project> redoHistory;
    std::vector<ClipboardClip> clipboard;
    std::vector<ClipboardMidiClip> midiClipboard;
    int clipboardBaseTrack = 0;
    double clipboardOriginSeconds = 0.0;
    double clipboardDurationSeconds = 0.0;
    std::optional<Project> trackMixGestureBefore;
    int trackMixGestureTrack = -1;
};
}
