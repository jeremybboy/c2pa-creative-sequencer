#include "ProjectEngine.h"

#include "ClipOcclusion.h"
#include "TracktionAdapter.h"
#include "project/MediaLibrary.h"
#include "project/ProjectSerializer.h"
#include "plugins/PluginDescriptor.h"
#include "provenance/ProvenanceService.h"
#include "transport/ArrangementLoop.h"
#include "transport/TransportFormatting.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace c2paseq
{
namespace
{
juce::String nextAudioTrackName(const Project& project)
{
    auto highestNumber = static_cast<int>(project.tracks.size());
    for (const auto& track : project.tracks)
    {
        if (! track.name.startsWith("Audio "))
            continue;
        const auto suffix = track.name.fromFirstOccurrenceOf("Audio ", false, false);
        if (suffix.containsOnly("0123456789"))
            highestNumber = std::max(highestNumber, suffix.getIntValue());
    }
    return "Audio " + juce::String(highestNumber + 1);
}
}

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
    clipboard.clear();
    trackMixGestureBefore.reset();
    trackMixGestureTrack = -1;
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

    for (auto& plugin : project->plugins)
    {
        const auto track = std::find_if(project->tracks.begin(), project->tracks.end(),
            [&](const auto& candidate) { return candidate.id == plugin.ownerId; });
        if (track == project->tracks.end())
            continue;
        const auto index = static_cast<int>(std::distance(project->tracks.begin(), track));
        juce::String state;
        bool bypassed = plugin.bypassed;
        bool missing = plugin.missing;
        if (tracktion.captureTrackPluginState(index, state, bypassed, missing).wasOk())
        {
            plugin.stateBase64 = state;
            plugin.bypassed = bypassed;
            plugin.missing = missing;
        }
    }

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
    project = std::move(loaded);
    paths = std::move(newPaths);
    undoHistory.clear();
    redoHistory.clear();
    clipboard.clear();
    trackMixGestureBefore.reset();
    trackMixGestureTrack = -1;
    if (auto result = rebuildEditFromProject(); result.failed())
    {
        closeProject();
        return result;
    }
    return juce::Result::ok();
}

void ProjectEngine::closeProject()
{
    project.reset();
    paths.reset();
    undoHistory.clear();
    redoHistory.clear();
    clipboard.clear();
    trackMixGestureBefore.reset();
    trackMixGestureTrack = -1;
    tracktion.closeProjectEdit();
}

void ProjectEngine::setBpm(double bpm)
{
    if (project.has_value())
        project->bpm = std::clamp(bpm, transport::minimumBpm, transport::maximumBpm);
}

void ProjectEngine::setLooping(bool shouldLoop, const juce::String& selectedClipId)
{
    if (! project.has_value())
    {
        tracktion.setLooping(false);
        return;
    }

    if (shouldLoop)
    {
        const auto range = resolveArrangementLoopRange(*project, selectedClipId);
        project->loopStartSeconds = range.startSeconds;
        project->loopEndSeconds = range.endSeconds;
        tracktion.setLoopRange(range.startSeconds, range.endSeconds);
    }
    project->looping = shouldLoop;
    tracktion.setLooping(shouldLoop);
}

juce::Result ProjectEngine::setLoopRangeAndEnable(double startSeconds,
                                                   double endSeconds)
{
    if (! project.has_value() || startSeconds < 0.0
        || endSeconds <= startSeconds + 0.001)
        return juce::Result::fail("Invalid loop selection");
    project->loopStartSeconds = startSeconds;
    project->loopEndSeconds = endSeconds;
    project->looping = true;
    tracktion.setLoopRange(startSeconds, endSeconds);
    tracktion.setLooping(true);
    return juce::Result::ok();
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

juce::Result ProjectEngine::deleteClips(const std::vector<juce::String>& clipIds)
{
    return mutateProject([&](Project& value)
    {
        auto removed = 0;
        for (auto& track : value.tracks)
        {
            const auto before = track.clips.size();
            std::erase_if(track.clips, [&](const auto& clip)
            {
                return std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end();
            });
            removed += static_cast<int>(before - track.clips.size());
        }
        return removed > 0 ? juce::Result::ok()
                           : juce::Result::fail("No selected clips were found");
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

juce::Result ProjectEngine::fillClipboardFromClips(
    const std::vector<juce::String>& clipIds)
{
    if (! project.has_value() || clipIds.empty())
        return juce::Result::fail("No clips are selected");

    clipboard.clear();
    auto minimumTrack = static_cast<int>(project->tracks.size());
    auto minimumStart = std::numeric_limits<double>::max();
    auto maximumEnd = 0.0;
    auto foundAny = false;
    for (std::size_t trackIndex = 0; trackIndex < project->tracks.size(); ++trackIndex)
        for (const auto& clip : project->tracks[trackIndex].clips)
            if (std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end())
            {
                foundAny = true;
                minimumTrack = std::min(minimumTrack, static_cast<int>(trackIndex));
                minimumStart = std::min(minimumStart, clip.startSeconds);
                maximumEnd = std::max(maximumEnd, clip.startSeconds + clip.lengthSeconds);
            }

    if (! foundAny)
        return juce::Result::fail("No selected clips were found");

    clipboardBaseTrack = minimumTrack;
    clipboardDurationSeconds = maximumEnd - minimumStart;
    for (std::size_t trackIndex = 0; trackIndex < project->tracks.size(); ++trackIndex)
        for (const auto& clip : project->tracks[trackIndex].clips)
            if (std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end())
                clipboard.push_back({ clip, static_cast<int>(trackIndex) - minimumTrack,
                                      clip.startSeconds - minimumStart });
    return juce::Result::ok();
}

juce::Result ProjectEngine::fillClipboardFromTimeRange(
    const ArrangementTimeSelection& selection)
{
    if (! project.has_value() || ! selection.isValid())
        return juce::Result::fail("No valid time selection");

    clipboard.clear();
    clipboardBaseTrack = selection.firstTrack;
    clipboardDurationSeconds = selection.endSeconds - selection.startSeconds;
    const auto lastTrack = std::min(selection.lastTrack,
        static_cast<int>(project->tracks.size()) - 1);
    for (auto trackIndex = selection.firstTrack; trackIndex <= lastTrack; ++trackIndex)
    {
        for (const auto& clip : project->tracks[static_cast<std::size_t>(trackIndex)].clips)
        {
            const auto overlapStart = std::max(selection.startSeconds, clip.startSeconds);
            const auto overlapEnd = std::min(selection.endSeconds,
                                             clip.startSeconds + clip.lengthSeconds);
            if (overlapEnd <= overlapStart + 0.001)
                continue;
            auto fragment = clip;
            fragment.startSeconds = overlapStart;
            fragment.sourceOffsetSeconds += overlapStart - clip.startSeconds;
            fragment.lengthSeconds = overlapEnd - overlapStart;
            clipboard.push_back({ fragment, trackIndex - selection.firstTrack,
                                  overlapStart - selection.startSeconds });
        }
    }
    return clipboard.empty() ? juce::Result::fail("The time selection contains no audio")
                             : juce::Result::ok();
}

juce::Result ProjectEngine::copyClips(const std::vector<juce::String>& clipIds)
{
    return fillClipboardFromClips(clipIds);
}

juce::Result ProjectEngine::copyTimeRange(const ArrangementTimeSelection& selection)
{
    return fillClipboardFromTimeRange(selection);
}

juce::Result ProjectEngine::cutClips(const std::vector<juce::String>& clipIds)
{
    if (auto result = fillClipboardFromClips(clipIds); result.failed())
        return result;
    return deleteClips(clipIds);
}

juce::Result ProjectEngine::cutTimeRange(const ArrangementTimeSelection& selection)
{
    if (auto result = fillClipboardFromTimeRange(selection); result.failed())
        return result;

    return mutateProject([&](Project& value)
    {
        const auto lastTrack = std::min(selection.lastTrack,
            static_cast<int>(value.tracks.size()) - 1);
        for (auto trackIndex = selection.firstTrack; trackIndex <= lastTrack; ++trackIndex)
        {
            auto& clips = value.tracks[static_cast<std::size_t>(trackIndex)].clips;
            std::vector<ClipModel> replacement;
            replacement.reserve(clips.size() + 2);
            for (const auto& clip : clips)
            {
                const auto clipEnd = clip.startSeconds + clip.lengthSeconds;
                const auto overlapStart = std::max(selection.startSeconds, clip.startSeconds);
                const auto overlapEnd = std::min(selection.endSeconds, clipEnd);
                if (overlapEnd <= overlapStart + 0.001)
                {
                    replacement.push_back(clip);
                    continue;
                }

                const auto leftLength = overlapStart - clip.startSeconds;
                const auto rightLength = clipEnd - overlapEnd;
                if (leftLength > 0.001)
                {
                    auto left = clip;
                    left.lengthSeconds = leftLength;
                    replacement.push_back(std::move(left));
                }
                if (rightLength > 0.001)
                {
                    auto right = clip;
                    if (leftLength > 0.001)
                        right.id = juce::Uuid().toString();
                    right.startSeconds = overlapEnd;
                    right.sourceOffsetSeconds += overlapEnd - clip.startSeconds;
                    right.lengthSeconds = rightLength;
                    replacement.push_back(std::move(right));
                }
            }
            clips = std::move(replacement);
        }
        return juce::Result::ok();
    });
}

juce::Result ProjectEngine::pasteClipboard(double destinationSeconds, int destinationTrack)
{
    if (! project.has_value() || clipboard.empty() || destinationSeconds < 0.0)
        return juce::Result::fail("Nothing valid is available to paste");
    const auto baseTrack = destinationTrack >= 0 ? destinationTrack : clipboardBaseTrack;
    return mutateProject([&](Project& value)
    {
        auto maximumTrack = baseTrack;
        for (const auto& item : clipboard)
            maximumTrack = std::max(maximumTrack, baseTrack + item.relativeTrack);
        ensureTrackCount(maximumTrack + 1);
        for (const auto& item : clipboard)
        {
            auto pasted = item.clip;
            pasted.id = juce::Uuid().toString();
            pasted.startSeconds = destinationSeconds + item.relativeStartSeconds;
            value.tracks[static_cast<std::size_t>(baseTrack + item.relativeTrack)]
                .clips.push_back(std::move(pasted));
        }
        return juce::Result::ok();
    });
}

juce::Result ProjectEngine::duplicateClips(const std::vector<juce::String>& clipIds)
{
    if (auto result = fillClipboardFromClips(clipIds); result.failed())
        return result;
    auto minimumStart = std::numeric_limits<double>::max();
    for (const auto& track : project->tracks)
        for (const auto& clip : track.clips)
            if (std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end())
                minimumStart = std::min(minimumStart, clip.startSeconds);
    return pasteClipboard(minimumStart + clipboardDurationSeconds, clipboardBaseTrack);
}

juce::Result ProjectEngine::duplicateTimeRange(const ArrangementTimeSelection& selection)
{
    if (auto result = fillClipboardFromTimeRange(selection); result.failed())
        return result;
    return pasteClipboard(selection.endSeconds, selection.firstTrack);
}

bool ProjectEngine::hasClipboard() const noexcept
{
    return ! clipboard.empty();
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

juce::Result ProjectEngine::addAudioTrack()
{
    return mutateProject([](Project& value)
    {
        TrackModel track;
        track.id = juce::Uuid().toString();
        track.name = nextAudioTrackName(value);
        value.tracks.push_back(std::move(track));
        return juce::Result::ok();
    });
}

juce::Result ProjectEngine::deleteAudioTrack(int trackIndex)
{
    return mutateProject([trackIndex](Project& value)
    {
        if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(value.tracks.size())))
            return juce::Result::fail("Invalid track");
        if (value.tracks.size() <= 1)
            return juce::Result::fail("A project must keep at least one audio track");

        const auto ownerId = value.tracks[static_cast<std::size_t>(trackIndex)].id;
        std::erase_if(value.plugins,
            [&](const auto& plugin) { return plugin.ownerId == ownerId; });
        value.tracks.erase(value.tracks.begin() + trackIndex);
        return juce::Result::ok();
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
    if (auto result = beginTrackMixGesture(trackIndex); result.failed())
        return result;
    if (auto result = previewTrackGain(trackIndex, gainDb); result.failed())
    {
        cancelTrackMixGesture();
        return result;
    }
    return endTrackMixGesture(trackIndex);
}

juce::Result ProjectEngine::setTrackPan(int trackIndex, double pan)
{
    if (auto result = beginTrackMixGesture(trackIndex); result.failed())
        return result;
    if (auto result = previewTrackPan(trackIndex, pan); result.failed())
    {
        cancelTrackMixGesture();
        return result;
    }
    return endTrackMixGesture(trackIndex);
}

juce::Result ProjectEngine::beginTrackMixGesture(int trackIndex)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return juce::Result::fail("Invalid track");
    if (trackMixGestureBefore.has_value())
        return trackMixGestureTrack == trackIndex
            ? juce::Result::ok()
            : juce::Result::fail("Another mixer gesture is active");
    trackMixGestureBefore = *project;
    trackMixGestureTrack = trackIndex;
    return juce::Result::ok();
}

juce::Result ProjectEngine::previewTrackGain(int trackIndex, double gainDb)
{
    if (! project.has_value() || ! trackMixGestureBefore.has_value()
        || trackIndex != trackMixGestureTrack)
        return juce::Result::fail("Track gain gesture is not active");
    const auto value = juce::jlimit(-60.0, 12.0, gainDb);
    if (auto result = tracktion.setTrackGain(trackIndex, value); result.failed())
        return result;
    project->tracks[static_cast<std::size_t>(trackIndex)].gainDb = value;
    return juce::Result::ok();
}

juce::Result ProjectEngine::previewTrackPan(int trackIndex, double pan)
{
    if (! project.has_value() || ! trackMixGestureBefore.has_value()
        || trackIndex != trackMixGestureTrack)
        return juce::Result::fail("Track pan gesture is not active");
    const auto value = juce::jlimit(-1.0, 1.0, pan);
    if (auto result = tracktion.setTrackPan(trackIndex, value); result.failed())
        return result;
    project->tracks[static_cast<std::size_t>(trackIndex)].pan = value;
    return juce::Result::ok();
}

juce::Result ProjectEngine::endTrackMixGesture(int trackIndex)
{
    if (! project.has_value() || ! trackMixGestureBefore.has_value()
        || trackIndex != trackMixGestureTrack)
        return juce::Result::fail("Track mixer gesture is not active");

    auto previous = std::move(*trackMixGestureBefore);
    trackMixGestureBefore.reset();
    trackMixGestureTrack = -1;
    const auto& before = previous.tracks[static_cast<std::size_t>(trackIndex)];
    const auto& after = project->tracks[static_cast<std::size_t>(trackIndex)];
    if (before.gainDb == after.gainDb && before.pan == after.pan)
        return juce::Result::ok();

    if (auto result = saveProject(); result.failed())
    {
        *project = std::move(previous);
        const auto& restored = project->tracks[static_cast<std::size_t>(trackIndex)];
        (void) tracktion.setTrackGain(trackIndex, restored.gainDb);
        (void) tracktion.setTrackPan(trackIndex, restored.pan);
        return result;
    }
    undoHistory.push_back(std::move(previous));
    redoHistory.clear();
    return juce::Result::ok();
}

juce::Result ProjectEngine::setTrackPlugin(int trackIndex,
                                            const PluginDescriptor& descriptor)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size()))
        || descriptor.format != "VST3" || descriptor.isInstrument)
        return juce::Result::fail("Invalid track VST3 audio effect");

    const auto previous = *project;
    if (auto result = tracktion.setTrackPlugin(trackIndex, descriptor.toJuce(), {}, false);
        result.failed())
    {
        (void) rebuildEditFromProject();
        return result;
    }

    PluginState state;
    state.ownerId = project->tracks[static_cast<std::size_t>(trackIndex)].id;
    state.pluginIdentifier = descriptor.identifier;
    state.name = descriptor.name;
    state.vendor = descriptor.vendor;
    state.version = descriptor.version;
    state.format = descriptor.format;
    state.category = descriptor.category;
    state.fileOrIdentifier = descriptor.fileOrIdentifier;
    state.uniqueId = descriptor.uniqueId;
    state.deprecatedUid = descriptor.deprecatedUid;
    state.isInstrument = descriptor.isInstrument;
    std::erase_if(project->plugins, [&](const auto& plugin) { return plugin.ownerId == state.ownerId; });
    project->plugins.push_back(std::move(state));

    if (auto result = saveProject(); result.failed())
    {
        *project = previous;
        (void) rebuildEditFromProject();
        return result;
    }
    undoHistory.push_back(previous);
    redoHistory.clear();
    return juce::Result::ok();
}

juce::Result ProjectEngine::setTrackPluginBypassed(int trackIndex, bool bypassed)
{
    auto* plugin = pluginForTrack(trackIndex);
    if (plugin == nullptr)
        return juce::Result::fail("Track has no VST3");
    if (plugin->missing)
    {
        plugin->bypassed = true;
        return juce::Result::ok();
    }

    const auto previous = *project;
    if (auto result = tracktion.setTrackPluginBypassed(trackIndex, bypassed); result.failed())
        return result;
    plugin->bypassed = bypassed;
    if (auto result = saveProject(); result.failed())
    {
        *project = previous;
        (void) tracktion.setTrackPluginBypassed(trackIndex, ! bypassed);
        return result;
    }
    undoHistory.push_back(previous);
    redoHistory.clear();
    return juce::Result::ok();
}

juce::Result ProjectEngine::removeTrackPlugin(int trackIndex)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return juce::Result::fail("Invalid track");
    const auto owner = project->tracks[static_cast<std::size_t>(trackIndex)].id;
    const auto found = std::find_if(project->plugins.begin(), project->plugins.end(),
        [&](const auto& plugin) { return plugin.ownerId == owner; });
    if (found == project->plugins.end())
        return juce::Result::fail("Track has no VST3");

    const auto previous = *project;
    if (! found->missing)
        if (auto result = tracktion.removeTrackPlugin(trackIndex); result.failed())
            return result;
    std::erase_if(project->plugins, [&](const auto& plugin) { return plugin.ownerId == owner; });
    if (auto result = saveProject(); result.failed())
    {
        *project = previous;
        (void) rebuildEditFromProject();
        return result;
    }
    undoHistory.push_back(previous);
    redoHistory.clear();
    return juce::Result::ok();
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
        if (const auto* plugin = pluginForTrack(static_cast<int>(snapshot.size())))
            trackSnapshot.plugin = TrackPluginSnapshot {
                plugin->pluginIdentifier, plugin->name, plugin->vendor,
                plugin->bypassed, plugin->missing
            };
        for (const auto& clip : track.clips)
        {
            const auto media = std::find_if(project->media.begin(), project->media.end(),
                [&clip](const auto& item) { return item.id == clip.mediaId; });
            if (media == project->media.end())
                continue;

            trackSnapshot.clips.push_back({
                clip.id,
                clip.mediaId,
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
        auto& track = project->tracks[trackIndex];
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

        if (auto* plugin = pluginForTrack(static_cast<int>(trackIndex)))
        {
            juce::PluginDescription description;
            description.name = plugin->name;
            description.descriptiveName = plugin->name;
            description.manufacturerName = plugin->vendor;
            description.version = plugin->version;
            description.pluginFormatName = plugin->format;
            description.category = plugin->category;
            description.fileOrIdentifier = plugin->fileOrIdentifier;
            description.uniqueId = plugin->uniqueId;
            description.deprecatedUid = plugin->deprecatedUid;
            description.isInstrument = plugin->isInstrument;
            const auto result = tracktion.setTrackPlugin(static_cast<int>(trackIndex),
                description, plugin->stateBase64, plugin->bypassed);
            plugin->missing = result.failed();
            if (plugin->missing)
                plugin->bypassed = true;
        }
    }
    auto range = ArrangementLoopRange { project->loopStartSeconds, project->loopEndSeconds };
    if (! range.isValid())
        range = resolveArrangementLoopRange(*project, {});
    project->loopStartSeconds = range.startSeconds;
    project->loopEndSeconds = range.endSeconds;
    tracktion.setLoopRange(range.startSeconds, range.endSeconds);
    tracktion.setLooping(project->looping);
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

void ProjectEngine::cancelTrackMixGesture()
{
    if (! project.has_value() || ! trackMixGestureBefore.has_value()
        || ! juce::isPositiveAndBelow(trackMixGestureTrack,
            static_cast<int>(trackMixGestureBefore->tracks.size())))
    {
        trackMixGestureBefore.reset();
        trackMixGestureTrack = -1;
        return;
    }
    const auto index = trackMixGestureTrack;
    *project = std::move(*trackMixGestureBefore);
    trackMixGestureBefore.reset();
    trackMixGestureTrack = -1;
    const auto& restored = project->tracks[static_cast<std::size_t>(index)];
    (void) tracktion.setTrackGain(index, restored.gainDb);
    (void) tracktion.setTrackPan(index, restored.pan);
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

PluginState* ProjectEngine::pluginForTrack(int trackIndex)
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return nullptr;
    const auto owner = project->tracks[static_cast<std::size_t>(trackIndex)].id;
    const auto found = std::find_if(project->plugins.begin(), project->plugins.end(),
        [&](const auto& plugin) { return plugin.ownerId == owner; });
    return found != project->plugins.end() ? &*found : nullptr;
}

const PluginState* ProjectEngine::pluginForTrack(int trackIndex) const
{
    if (! project.has_value()
        || ! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project->tracks.size())))
        return nullptr;
    const auto owner = project->tracks[static_cast<std::size_t>(trackIndex)].id;
    const auto found = std::find_if(project->plugins.begin(), project->plugins.end(),
        [&](const auto& plugin) { return plugin.ownerId == owner; });
    return found != project->plugins.end() ? &*found : nullptr;
}
}
