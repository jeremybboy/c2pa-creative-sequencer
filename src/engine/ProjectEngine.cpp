#include "ProjectEngine.h"

#include "TracktionAdapter.h"
#include "project/ProjectSerializer.h"
#include "transport/TransportFormatting.h"

#include <algorithm>

namespace c2paseq
{
ProjectEngine::ProjectEngine(TracktionAdapter& tracktionAdapter)
    : tracktion(tracktionAdapter)
{
}

juce::Result ProjectEngine::createProject(const juce::File& projectFolder,
                                           const juce::String& projectName)
{
    ProjectPaths newPaths(projectFolder);
    if (newPaths.projectJson().existsAsFile())
        return juce::Result::fail("Project already exists; open it instead");
    if (auto result = newPaths.createDirectories(); result.failed())
        return result;

    auto newProject = Project::create(projectName);
    if (! tracktion.createProjectEdit(newPaths.arrangementEdit()))
        return juce::Result::fail("Could not create Tracktion arrangement");

    project = std::move(newProject);
    paths = std::move(newPaths);
    if (auto result = saveProject(); result.failed())
    {
        closeProject();
        return result;
    }
    return juce::Result::ok();
}

juce::Result ProjectEngine::saveProject()
{
    if (! project.has_value() || ! paths.has_value())
        return juce::Result::fail("No project is open");

    project->modifiedAt = juce::Time::getCurrentTime().toISO8601(true);
    project->bpm = tracktion.transportSnapshot().bpm;
    if (! tracktion.saveProjectEdit(paths->arrangementEdit()))
        return juce::Result::fail("Could not save arrangement.tracktionedit");
    return ProjectSerializer::save(*project, *paths);
}

juce::Result ProjectEngine::openProject(const juce::File& projectFolder)
{
    ProjectPaths newPaths(projectFolder);
    Project loaded;
    if (auto result = ProjectSerializer::load(newPaths, loaded); result.failed())
        return result;
    if (! tracktion.loadProjectEdit(newPaths.arrangementEdit()))
        return juce::Result::fail("Could not load arrangement.tracktionedit");

    project = std::move(loaded);
    paths = std::move(newPaths);
    tracktion.setBpm(project->bpm);
    return juce::Result::ok();
}

void ProjectEngine::closeProject()
{
    project.reset();
    paths.reset();
    tracktion.closeProjectEdit();
}

void ProjectEngine::setBpm(double bpm)
{
    if (project.has_value())
        project->bpm = std::clamp(bpm, transport::minimumBpm, transport::maximumBpm);
}

bool ProjectEngine::hasProject() const noexcept
{
    return project.has_value();
}

juce::String ProjectEngine::displayName() const
{
    return project.has_value() ? project->name : "No project";
}

const Project* ProjectEngine::currentProject() const noexcept
{
    return project.has_value() ? &*project : nullptr;
}

const ProjectPaths* ProjectEngine::currentPaths() const noexcept
{
    return paths.has_value() ? &*paths : nullptr;
}
}
