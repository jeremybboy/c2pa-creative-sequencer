#pragma once

#include "Project.h"
#include "ProjectPaths.h"

namespace c2paseq
{
class ProjectSerializer final
{
public:
    [[nodiscard]] static juce::Result save(const Project& project, const ProjectPaths& paths);
    [[nodiscard]] static juce::Result load(const ProjectPaths& paths, Project& project);
};
}
