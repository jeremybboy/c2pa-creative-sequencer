#include "ProjectPaths.h"

namespace c2paseq
{
ProjectPaths::ProjectPaths(juce::File projectDirectory)
    : projectRoot(std::move(projectDirectory))
{
}

const juce::File& ProjectPaths::root() const noexcept
{
    return projectRoot;
}

juce::File ProjectPaths::projectJson() const
{
    return projectRoot.getChildFile("project.json");
}

juce::File ProjectPaths::arrangementEdit() const
{
    return projectRoot.getChildFile("arrangement.tracktionedit");
}

juce::File ProjectPaths::provenanceJson() const
{
    return projectRoot.getChildFile("provenance.json");
}

juce::File ProjectPaths::mediaDirectory() const
{
    return projectRoot.getChildFile("Media");
}

juce::Result ProjectPaths::createDirectories() const
{
    if (! hasProjectExtension(projectRoot))
        return juce::Result::fail("Project folder must end in .c2paseq");

    auto result = projectRoot.createDirectory();
    if (result.failed())
        return result;

    return mediaDirectory().createDirectory();
}

bool ProjectPaths::hasProjectExtension(const juce::File& file)
{
    return file.hasFileExtension("c2paseq");
}
}
