#include "RenderService.h"

#include "engine/ClipOcclusion.h"
#include "engine/TracktionAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <juce_cryptography/juce_cryptography.h>

namespace c2paseq
{
juce::Result RenderService::createMidiStemPlan(const Project& project, int trackIndex,
                                               MidiStemPlan& plan)
{
    if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(project.tracks.size()))
        || project.tracks[static_cast<std::size_t>(trackIndex)].type != TrackType::midi)
        return juce::Result::fail("Select a MIDI track to bounce");
    if (! std::isfinite(project.bpm) || project.bpm <= 0.0)
        return juce::Result::fail("The project tempo is invalid");
    const auto& track = project.tracks[static_cast<std::size_t>(trackIndex)];
    const auto plugin = std::find_if(project.plugins.begin(), project.plugins.end(),
        [&](const auto& item) { return item.ownerId == track.id; });
    if (plugin == project.plugins.end() || ! plugin->isInstrument
        || plugin->format != "VST3" || plugin->bypassed || plugin->missing)
        return juce::Result::fail("The MIDI track needs an active VST3 instrument");
    juce::MemoryBlock pluginState;
    if (! pluginState.fromBase64Encoding(plugin->stateBase64) || pluginState.isEmpty())
        return juce::Result::fail("The instrument did not provide a restorable state");

    juce::Array<juce::var> clips;
    int noteCount = 0;
    double endSeconds = 0.0;
    double startSeconds = std::numeric_limits<double>::max();
    for (const auto& clip : track.midiClips)
    {
        juce::Array<juce::var> notes;
        for (const auto& note : clip.notes)
        {
            auto* item = new juce::DynamicObject();
            item->setProperty("pitch", note.noteNumber);
            item->setProperty("startBeats", note.start.beats);
            item->setProperty("durationBeats", note.duration.beats);
            item->setProperty("velocity", note.velocity);
            notes.add(juce::var(item));
            ++noteCount;
        }
        auto* item = new juce::DynamicObject();
        item->setProperty("startBeats", clip.start.beats);
        item->setProperty("lengthBeats", clip.length.beats);
        item->setProperty("notes", notes);
        clips.add(juce::var(item));
        if (! clip.notes.empty())
        {
            startSeconds = std::min(startSeconds, clip.start.beats * 60.0 / project.bpm);
            endSeconds = std::max(endSeconds,
                (clip.start.beats + clip.length.beats) * 60.0 / project.bpm);
        }
    }
    if (noteCount == 0 || endSeconds <= 0.0)
        return juce::Result::fail("The MIDI track has no notes to render");
    const auto midiJson = juce::JSON::toString(juce::var(clips), true);
    const auto midiHash = juce::SHA256(midiJson.toRawUTF8(),
        midiJson.getNumBytesAsUTF8()).toHexString();
    auto* instrument = new juce::DynamicObject();
    instrument->setProperty("name", plugin->name);
    instrument->setProperty("vendor", plugin->vendor);
    instrument->setProperty("version", plugin->version);
    instrument->setProperty("format", plugin->format);
    instrument->setProperty("identifier", plugin->pluginIdentifier);
    instrument->setProperty("stateSha256", juce::SHA256(pluginState).toHexString());
    auto* metadata = new juce::DynamicObject();
    metadata->setProperty("trackName", track.name);
    metadata->setProperty("bpm", project.bpm);
    metadata->setProperty("midiContentSha256", midiHash);
    metadata->setProperty("clipCount", static_cast<int>(track.midiClips.size()));
    metadata->setProperty("noteCount", noteCount);
    metadata->setProperty("instrument", juce::var(instrument));
    metadata->setProperty("gainDb", track.gainDb);
    metadata->setProperty("pan", track.pan);
    metadata->setProperty("gainPanPrinted", false);
    metadata->setProperty("masterProcessingPrinted", false);
    metadata->setProperty("startSeconds", startSeconds);
    metadata->setProperty("endSeconds", endSeconds);
    metadata->setProperty("midiHashEncoding", "juce-compact-json-clips-v1");
    auto* parameters = new juce::DynamicObject();
    parameters->setProperty("c2paseq:midiRender", juce::var(metadata));
    const juce::var parameterValue(parameters);
    const auto signatureJson = juce::JSON::toString(parameterValue, true);
    MidiStemPlan candidate;
    candidate.trackIndex = trackIndex;
    candidate.projectId = project.id;
    candidate.trackId = track.id;
    candidate.trackName = track.name;
    candidate.sourceSignature = juce::SHA256(signatureJson.toRawUTF8(),
        signatureJson.getNumBytesAsUTF8()).toHexString();
    candidate.endSeconds = endSeconds;
    candidate.startSeconds = startSeconds;
    parameters->setProperty("c2paseq:sourceSignature", candidate.sourceSignature);
    candidate.descriptor.title = juce::File::createLegalFileName(track.name + " Stem") + ".wav";
    candidate.descriptor.actions = makeMidiRenderedStemActions(parameterValue);
    plan = std::move(candidate);
    return juce::Result::ok();
}

juce::Result RenderService::createPlan(const Project& project,
                                       const ProjectPaths& paths,
                                       RenderPlan& plan)
{
    RenderPlan candidate;
    const auto anySolo = std::any_of(project.tracks.begin(), project.tracks.end(),
                                     [](const auto& track) { return track.soloed; });
    std::vector<juce::String> includedMedia;

    for (const auto& track : project.tracks)
    {
        if (track.muted || (anySolo && ! track.soloed))
            continue;

        for (const auto& segment : buildPlaybackClipSegments(track.clips))
        {
            const auto& clip = track.clips[segment.clipIndex];
            candidate.endSeconds = std::max(candidate.endSeconds,
                segment.startSeconds + segment.lengthSeconds);
            if (std::find(includedMedia.begin(), includedMedia.end(), clip.mediaId)
                != includedMedia.end())
                continue;

            const auto media = std::find_if(project.media.begin(), project.media.end(),
                [&](const auto& item) { return item.id == clip.mediaId; });
            if (media == project.media.end())
                return juce::Result::fail("An audible clip references missing project media");
            const auto file = paths.root().getChildFile(media->relativePath);
            if (! file.existsAsFile())
                return juce::Result::fail("Contributing source media is missing");
            includedMedia.push_back(media->id);
            candidate.ingredients.push_back({ media->id, media->originalFileName,
                                               media->sha256, file, media->provenance });
        }

        if (track.type == TrackType::midi)
        {
            const auto plugin = std::find_if(project.plugins.begin(), project.plugins.end(),
                [&](const auto& item) { return item.ownerId == track.id; });
            if (plugin == project.plugins.end() || ! plugin->isInstrument
                || plugin->bypassed || plugin->missing)
                continue;
            for (const auto& clip : track.midiClips)
            {
                if (clip.notes.empty())
                    continue;
                candidate.endSeconds = std::max(candidate.endSeconds,
                    (clip.start.beats + clip.length.beats) * 60.0 / project.bpm);
            }
        }
    }

    if (candidate.endSeconds <= 0.0)
        return juce::Result::fail("The project has no audible clips to export");
    plan = std::move(candidate);
    return juce::Result::ok();
}

juce::Result RenderService::render(TracktionAdapter& tracktion,
                                   const RenderPlan& plan,
                                   const juce::File& destination)
{
    if (! destination.hasFileExtension("wav"))
        return juce::Result::fail("Export destination must be a WAV file");
    return tracktion.renderWav(destination, plan.endSeconds);
}
}
