#include "ProjectEngine.h"

#include "ClipOcclusion.h"
#include "TracktionAdapter.h"
#include "project/MediaLibrary.h"
#include "project/ProjectSerializer.h"
#include "provenance/ProvenanceService.h"
#include "transport/TransportFormatting.h"

#include <algorithm>
#include <cmath>

namespace c2paseq
{
ProjectEngine::ProjectEngine(TracktionAdapter& tracktionAdapter,
                             ProvenanceService& provenanceService)
    : tracktion(tracktionAdapter), provenance(provenanceService)
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
    for (int index = 0; index < 4; ++index)
    {
        TrackModel track;
        track.id = juce::Uuid().toString();
        track.name = "Audio " + juce::String(index + 1);
        newProject.tracks.push_back(std::move(track));
    }
    if (! tracktion.createProjectEdit(newPaths.arrangementEdit()))
        return juce::Result::fail("Could not create Tracktion arrangement");

    project = std::move(newProject);
    paths = std::move(newPaths);
    undoHistory.clear();
    redoHistory.clear();
    if (auto result = rebuildEditFromProject(); result.failed())
    {
        closeProject();
        return result;
    }
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
    undoHistory.clear();
    redoHistory.clear();
    tracktion.setBpm(project->bpm);
    return juce::Result::ok();
}

void ProjectEngine::closeProject()
{
    project.reset();
    paths.reset();
    undoHistory.clear();
    redoHistory.clear();
    tracktion.closeProjectEdit();
}

void ProjectEngine::setBpm(double bpm)
{
    if (project.has_value())
        project->bpm = std::clamp(bpm, transport::minimumBpm, transport::maximumBpm);
}

juce::Result ProjectEngine::importAudio(const juce::File& source,
                                        int trackIndex,
                                        double startSeconds)
{
    if (! project.has_value() || ! paths.has_value() || trackIndex < 0)
        return juce::Result::fail("Create or open a project before importing audio");

    AudioFileMetadata metadata;
    if (auto result = tracktion.inspectAudioFile(source, metadata); result.failed())
        return result;
    const auto sourceProvenance = provenance.inspect(source);

    const auto previous = *project;
    MediaReference media;
    bool mediaWasAdded = false;
    if (auto result = MediaLibrary::copySourceIntoProject(
            *project, *paths, source, media, &mediaWasAdded);
        result.failed())
        return result;

    const auto copiedFile = paths->root().getChildFile(media.relativePath);
    media.provenance = sourceProvenance;
    const auto registered = std::find_if(project->media.begin(), project->media.end(),
        [&](const auto& item) { return item.id == media.id; });
    if (registered != project->media.end())
        registered->provenance = media.provenance;
    ensureTrackCount(trackIndex + 1);

    ClipModel clip;
    clip.id = juce::Uuid().toString();
    clip.mediaId = media.id;
    clip.startSeconds = std::max(0.0, startSeconds);
    clip.lengthSeconds = metadata.lengthSeconds;

    project->tracks[static_cast<std::size_t>(trackIndex)].clips.push_back(std::move(clip));
    const auto result = commitMutation(previous);
    if (result.failed() && mediaWasAdded)
        copiedFile.deleteFile();
    return result;
}

juce::Result ProjectEngine::moveClip(const juce::String& clipId,
                                     int trackIndex,
                                     double startSeconds)
{
    return mutateProject([&](Project& value)
    {
        if (trackIndex < 0)
            return juce::Result::fail("Invalid target track");
        ensureTrackCount(trackIndex + 1);
        for (auto& track : value.tracks)
        {
            const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                [&](const auto& clip) { return clip.id == clipId; });
            if (found == track.clips.end())
                continue;
            auto moved = *found;
            moved.startSeconds = std::max(0.0, startSeconds);
            track.clips.erase(found);
            value.tracks[static_cast<std::size_t>(trackIndex)].clips.push_back(std::move(moved));
            return juce::Result::ok();
        }
        return juce::Result::fail("Clip was not found");
    });
}

juce::Result ProjectEngine::trimClip(const juce::String& clipId,
                                     double startSeconds,
                                     double sourceOffsetSeconds,
                                     double lengthSeconds)
{
    return mutateProject([&](Project& value)
    {
        if (startSeconds < 0.0 || sourceOffsetSeconds < 0.0 || lengthSeconds <= 0.001)
            return juce::Result::fail("Invalid clip trim");
        for (auto& track : value.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    clip.startSeconds = startSeconds;
                    clip.sourceOffsetSeconds = sourceOffsetSeconds;
                    clip.lengthSeconds = lengthSeconds;
                    return juce::Result::ok();
                }
        return juce::Result::fail("Clip was not found");
    });
}

juce::Result ProjectEngine::deleteClip(const juce::String& clipId)
{
    return mutateProject([&](Project& value)
    {
        for (auto& track : value.tracks)
        {
            const auto before = track.clips.size();
            std::erase_if(track.clips, [&](const auto& clip) { return clip.id == clipId; });
            if (track.clips.size() != before)
                return juce::Result::ok();
        }
        return juce::Result::fail("Clip was not found");
    });
}

juce::Result ProjectEngine::duplicateClip(const juce::String& clipId)
{
    return mutateProject([&](Project& value)
    {
        for (auto& track : value.tracks)
        {
            const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                [&](const auto& clip) { return clip.id == clipId; });
            if (found == track.clips.end())
                continue;
            auto duplicate = *found;
            duplicate.id = juce::Uuid().toString();
            duplicate.startSeconds += duplicate.lengthSeconds;
            track.clips.push_back(std::move(duplicate));
            return juce::Result::ok();
        }
        return juce::Result::fail("Clip was not found");
    });
}

juce::Result ProjectEngine::splitClip(const juce::String& clipId,
                                      double positionSeconds)
{
    return mutateProject([&](Project& value)
    {
        for (auto& track : value.tracks)
        {
            const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                [&](const auto& clip) { return clip.id == clipId; });
            if (found == track.clips.end())
                continue;
            const auto firstLength = positionSeconds - found->startSeconds;
            if (firstLength <= 0.001 || firstLength >= found->lengthSeconds - 0.001)
                return juce::Result::fail("Playhead must be inside the selected clip");
            auto right = *found;
            right.id = juce::Uuid().toString();
            right.startSeconds = positionSeconds;
            right.sourceOffsetSeconds += firstLength;
            right.lengthSeconds -= firstLength;
            found->lengthSeconds = firstLength;
            track.clips.push_back(std::move(right));
            return juce::Result::ok();
        }
        return juce::Result::fail("Clip was not found");
    });
}

juce::Result ProjectEngine::setTrackName(int trackIndex, const juce::String& name)
{
    return mutateProject([&](Project& value)
    {
        if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(value.tracks.size()))
            || name.trim().isEmpty())
            return juce::Result::fail("Invalid track name");
        value.tracks[static_cast<std::size_t>(trackIndex)].name = name.trim();
        return juce::Result::ok();
    });
}

juce::Result ProjectEngine::setTrackMute(int trackIndex, bool muted)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return juce::Result::fail("Invalid track");
    auto previous = *project;
    project->tracks[static_cast<std::size_t>(trackIndex)].muted = muted;
    return commitLiveTrackAudibility(std::move(previous), trackIndex, false);
}

juce::Result ProjectEngine::setTrackSolo(int trackIndex, bool soloed)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return juce::Result::fail("Invalid track");
    auto previous = *project;
    project->tracks[static_cast<std::size_t>(trackIndex)].soloed = soloed;
    return commitLiveTrackAudibility(std::move(previous), trackIndex, true);
}

juce::Result ProjectEngine::setTrackGain(int trackIndex, double gainDb)
{
    return mutateProject([&](Project& value)
    {
        if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(value.tracks.size())))
            return juce::Result::fail("Invalid track");
        value.tracks[static_cast<std::size_t>(trackIndex)].gainDb = juce::jlimit(-60.0, 12.0, gainDb);
        return juce::Result::ok();
    });
}

juce::Result ProjectEngine::setTrackPan(int trackIndex, double pan)
{
    return mutateProject([&](Project& value)
    {
        if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(value.tracks.size())))
            return juce::Result::fail("Invalid track");
        value.tracks[static_cast<std::size_t>(trackIndex)].pan = juce::jlimit(-1.0, 1.0, pan);
        return juce::Result::ok();
    });
}

bool ProjectEngine::undo()
{
    if (! project.has_value() || undoHistory.empty())
        return false;
    auto current = *project;
    *project = undoHistory.back();
    undoHistory.pop_back();
    if (rebuildEditFromProject().failed() || saveProject().failed())
    {
        *project = std::move(current);
        (void) rebuildEditFromProject();
        return false;
    }
    redoHistory.push_back(std::move(current));
    return true;
}

bool ProjectEngine::redo()
{
    if (! project.has_value() || redoHistory.empty())
        return false;
    auto current = *project;
    *project = redoHistory.back();
    redoHistory.pop_back();
    if (rebuildEditFromProject().failed() || saveProject().failed())
    {
        *project = std::move(current);
        (void) rebuildEditFromProject();
        return false;
    }
    undoHistory.push_back(std::move(current));
    return true;
}

bool ProjectEngine::canUndo() const noexcept { return ! undoHistory.empty(); }
bool ProjectEngine::canRedo() const noexcept { return ! redoHistory.empty(); }

void ProjectEngine::setTimelineView(double pixelsPerSecond, double scrollSeconds)
{
    if (! project.has_value())
        return;
    project->timelinePixelsPerSecond = juce::jlimit(24.0, 640.0, pixelsPerSecond);
    project->timelineScrollSeconds = std::max(0.0, scrollSeconds);
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
        trackSnapshot.id = track.id;
        trackSnapshot.name = track.name;
        trackSnapshot.gainDb = track.gainDb;
        trackSnapshot.pan = track.pan;
        trackSnapshot.muted = track.muted;
        trackSnapshot.soloed = track.soloed;
        for (const auto& clip : track.clips)
        {
            const auto media = std::find_if(project->media.begin(), project->media.end(),
                [&clip](const auto& item) { return item.id == clip.mediaId; });
            if (media == project->media.end())
                continue;

            trackSnapshot.clips.push_back({
                clip.id,
                media->originalFileName,
                paths->root().getChildFile(media->relativePath),
                clip.startSeconds,
                clip.sourceOffsetSeconds,
                clip.lengthSeconds,
                media->provenance
            });
        }
        snapshot.push_back(std::move(trackSnapshot));
    }
    return snapshot;
}

juce::Result ProjectEngine::rebuildEditFromProject()
{
    if (! project.has_value() || ! paths.has_value())
        return juce::Result::fail("No project is open");
    if (! tracktion.createProjectEdit(paths->arrangementEdit()))
        return juce::Result::fail("Could not rebuild Tracktion arrangement");
    tracktion.setBpm(project->bpm);

    for (std::size_t trackIndex = 0; trackIndex < project->tracks.size(); ++trackIndex)
    {
        const auto& track = project->tracks[trackIndex];
        if (auto result = tracktion.setTrackProperties(static_cast<int>(trackIndex), track.name,
                track.gainDb, track.pan, track.muted, track.soloed); result.failed())
            return result;
        for (const auto& segment : buildPlaybackClipSegments(track.clips))
        {
            const auto& clip = track.clips[segment.clipIndex];
            const auto media = std::find_if(project->media.begin(), project->media.end(),
                [&](const auto& item) { return item.id == clip.mediaId; });
            if (media == project->media.end())
                return juce::Result::fail("Clip media is missing");
            const auto file = paths->root().getChildFile(media->relativePath);
            if (auto result = tracktion.insertAudioClip(file, media->originalFileName,
                    static_cast<int>(trackIndex), segment.startSeconds,
                    segment.sourceOffsetSeconds, segment.lengthSeconds); result.failed())
                return result;
        }
    }
    return juce::Result::ok();
}

juce::Result ProjectEngine::commitMutation(Project previous)
{
    if (auto result = rebuildEditFromProject(); result.failed())
    {
        *project = std::move(previous);
        (void) rebuildEditFromProject();
        return result;
    }
    if (auto result = saveProject(); result.failed())
    {
        *project = std::move(previous);
        (void) rebuildEditFromProject();
        return result;
    }
    undoHistory.push_back(std::move(previous));
    redoHistory.clear();
    return juce::Result::ok();
}

juce::Result ProjectEngine::mutateProject(
    const std::function<juce::Result(Project&)>& mutation)
{
    if (! project.has_value())
        return juce::Result::fail("No project is open");
    auto previous = *project;
    if (auto result = mutation(*project); result.failed())
    {
        *project = std::move(previous);
        return result;
    }
    return commitMutation(std::move(previous));
}

juce::Result ProjectEngine::commitLiveTrackAudibility(Project previous,
                                                       int trackIndex,
                                                       bool solo)
{
    const auto& updated = project->tracks[static_cast<std::size_t>(trackIndex)];
    auto result = solo ? tracktion.setTrackSolo(trackIndex, updated.soloed)
                       : tracktion.setTrackMute(trackIndex, updated.muted);
    if (result.wasOk())
        result = saveProject();

    if (result.failed())
    {
        *project = std::move(previous);
        const auto& restored = project->tracks[static_cast<std::size_t>(trackIndex)];
        if (solo)
            (void) tracktion.setTrackSolo(trackIndex, restored.soloed);
        else
            (void) tracktion.setTrackMute(trackIndex, restored.muted);
        return result;
    }

    undoHistory.push_back(std::move(previous));
    redoHistory.clear();
    return juce::Result::ok();
}

void ProjectEngine::ensureTrackCount(int count)
{
    if (! project.has_value())
        return;
    while (static_cast<int>(project->tracks.size()) < count)
    {
        TrackModel track;
        track.id = juce::Uuid().toString();
        track.name = "Audio " + juce::String(project->tracks.size() + 1);
        project->tracks.push_back(std::move(track));
    }
}
}
