#pragma once

#include "project/Project.h"
#include "project/ProjectPaths.h"

#include <optional>
#include <functional>
#include <vector>

namespace c2paseq
{
class TracktionAdapter;
class ProvenanceService;

struct ArrangementClipSnapshot
{
    juce::String id;
    juce::String name;
    juce::File mediaFile;
    double startSeconds = 0.0;
    double sourceOffsetSeconds = 0.0;
    double lengthSeconds = 0.0;
    IngredientInfo provenance;
};

struct ArrangementTrackSnapshot
{
    juce::String id;
    juce::String name;
    double gainDb = 0.0;
    double pan = 0.0;
    bool muted = false;
    bool soloed = false;
    std::vector<ArrangementClipSnapshot> clips;
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
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           int trackIndex,
                                           double startSeconds);
    [[nodiscard]] juce::Result moveClip(const juce::String& clipId,
                                        int trackIndex,
                                        double startSeconds);
    [[nodiscard]] juce::Result trimClip(const juce::String& clipId,
                                        double startSeconds,
                                        double sourceOffsetSeconds,
                                        double lengthSeconds);
    [[nodiscard]] juce::Result deleteClip(const juce::String& clipId);
    [[nodiscard]] juce::Result duplicateClip(const juce::String& clipId);
    [[nodiscard]] juce::Result splitClip(const juce::String& clipId,
                                         double positionSeconds);
    [[nodiscard]] juce::Result setTrackName(int trackIndex, const juce::String& name);
    [[nodiscard]] juce::Result setTrackMute(int trackIndex, bool muted);
    [[nodiscard]] juce::Result setTrackSolo(int trackIndex, bool soloed);
    [[nodiscard]] juce::Result setTrackGain(int trackIndex, double gainDb);
    [[nodiscard]] juce::Result setTrackPan(int trackIndex, double pan);
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
    void ensureTrackCount(int count);

    TracktionAdapter& tracktion;
    ProvenanceService& provenance;
    std::optional<Project> project;
    std::optional<ProjectPaths> paths;
    std::vector<Project> undoHistory;
    std::vector<Project> redoHistory;
};
}
