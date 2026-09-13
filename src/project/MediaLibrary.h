#pragma once

#include "Project.h"
#include "ProjectPaths.h"

namespace c2paseq
{
class MediaLibrary final
{
public:
    [[nodiscard]] static juce::Result copySourceIntoProject(
        Project& project,
        const ProjectPaths& paths,
        const juce::File& source,
        MediaReference& reference,
        bool* added = nullptr);
};
}
