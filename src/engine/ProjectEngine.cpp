#include "ProjectEngine.h"

#include "TracktionAdapter.h"
#include "project/MediaLibrary.h"
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

juce::Result ProjectEngine::importAudio(const juce::File& source,
                                        double startSeconds)
{
    if (! project.has_value() || ! paths.has_value())
        return juce::Result::fail("Create or open a project before importing audio");

    AudioFileMetadata metadata;
    if (auto result = tracktion.inspectAudioFile(source, metadata); result.failed())
        return result;

    MediaReference media;
    bool mediaWasAdded = false;
    if (auto result = MediaLibrary::copySourceIntoProject(
            *project, *paths, source, media, &mediaWasAdded);
        result.failed())
        return result;

    const auto copiedFile = paths->root().getChildFile(media.relativePath);
    const auto trackName = source.getFileNameWithoutExtension();
    const auto trackIndex = static_cast<int>(project->tracks.size());
    if (auto result = tracktion.insertAudioClip(copiedFile, trackName, trackIndex,
                                                 std::max(0.0, startSeconds),
                                                 metadata.lengthSeconds);
        result.failed())
    {
        if (mediaWasAdded)
        {
            project->media.pop_back();
            copiedFile.deleteFile();
        }
        return result;
    }

    ClipModel clip;
    clip.id = juce::Uuid().toString();
    clip.mediaId = media.id;
    clip.startSeconds = std::max(0.0, startSeconds);
    clip.lengthSeconds = metadata.lengthSeconds;

    TrackModel track;
    track.id = juce::Uuid().toString();
    track.name = trackName;
    track.clips.push_back(std::move(clip));
    project->tracks.push_back(std::move(track));

    return saveProject();
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

std::vector<ArrangementTrackSnapshot> ProjectEngine::arrangementSnapshot() const
{
    std::vector<ArrangementTrackSnapshot> snapshot;
    if (! project.has_value() || ! paths.has_value())
        return snapshot;

    for (const auto& track : project->tracks)
    {
        ArrangementTrackSnapshot trackSnapshot;
        trackSnapshot.name = track.name;
        for (const auto& clip : track.clips)
        {
            const auto media = std::find_if(project->media.begin(), project->media.end(),
                [&clip](const auto& item) { return item.id == clip.mediaId; });
            if (media == project->media.end())
                continue;

            trackSnapshot.clips.push_back({
                media->originalFileName,
                paths->root().getChildFile(media->relativePath),
                clip.startSeconds,
                clip.lengthSeconds
            });
        }
        snapshot.push_back(std::move(trackSnapshot));
    }
    return snapshot;
}
}
