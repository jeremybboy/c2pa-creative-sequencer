#pragma once

#include "project/Project.h"
#include "project/ProjectPaths.h"

#include <optional>
#include <vector>

namespace c2paseq
{
class TracktionAdapter;

struct ArrangementClipSnapshot
{
    juce::String name;
    juce::File mediaFile;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
};

struct ArrangementTrackSnapshot
{
    juce::String name;
    std::vector<ArrangementClipSnapshot> clips;
};

class ProjectEngine final
{
public:
    explicit ProjectEngine(TracktionAdapter& tracktionAdapter);

    [[nodiscard]] juce::Result createProject(const juce::File& projectFolder,
                                             const juce::String& projectName);
    [[nodiscard]] juce::Result saveProject();
    [[nodiscard]] juce::Result openProject(const juce::File& projectFolder);
    void closeProject();
    void setBpm(double bpm);
    [[nodiscard]] juce::Result importAudio(const juce::File& source,
                                           double startSeconds);

    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String displayName() const;
    [[nodiscard]] const Project* currentProject() const noexcept;
    [[nodiscard]] const ProjectPaths* currentPaths() const noexcept;
    [[nodiscard]] std::vector<ArrangementTrackSnapshot> arrangementSnapshot() const;

private:
    TracktionAdapter& tracktion;
    std::optional<Project> project;
    std::optional<ProjectPaths> paths;
};
}
