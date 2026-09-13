#pragma once

#include "project/Project.h"
#include "project/ProjectPaths.h"

#include <optional>

namespace c2paseq
{
class TracktionAdapter;

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

    [[nodiscard]] bool hasProject() const noexcept;
    [[nodiscard]] juce::String displayName() const;
    [[nodiscard]] const Project* currentProject() const noexcept;
    [[nodiscard]] const ProjectPaths* currentPaths() const noexcept;

private:
    TracktionAdapter& tracktion;
    std::optional<Project> project;
    std::optional<ProjectPaths> paths;
};
}
