#include "project/MediaLibrary.h"
#include "project/ProjectSerializer.h"

#include <juce_cryptography/juce_cryptography.h>

#include <iostream>
#include <tuple>

namespace
{
struct TemporaryDirectory
{
    TemporaryDirectory()
        : directory(juce::File::getCurrentWorkingDirectory()
                        .getChildFile("c2paseq-project-model-test-" + juce::Uuid().toString()))
    {
        const auto result = directory.createDirectory();
        ready = result.wasOk() && directory.isDirectory();
    }

    ~TemporaryDirectory()
    {
        directory.deleteRecursively();
    }

    juce::File directory;
    bool ready = false;
};

int fail(int code, const juce::String& message)
{
    std::cerr << message << '\n';
    return code;
}
}

int main()
{
    TemporaryDirectory temporary;
    if (! temporary.ready)
        return fail(1, "could not create temporary test directory: "
                       + temporary.directory.getFullPathName());
    const c2paseq::ProjectPaths paths(
        temporary.directory.getChildFile("Round Trip.c2paseq"));

    auto project = c2paseq::Project::create("Round Trip");
    project.bpm = 127.5;
    project.timelinePixelsPerSecond = 144.0;
    project.timelineScrollSeconds = 9.5;
    project.loopStartSeconds = 4.0;
    project.loopEndSeconds = 12.5;
    project.looping = true;

    c2paseq::TrackModel track;
    track.id = juce::Uuid().toString();
    track.name = "Stem 1";
    track.type = c2paseq::TrackType::audio;
    track.gainDb = -2.5;
    track.pan = 0.25;
    track.muted = true;

    const auto source = temporary.directory.getChildFile("source.wav");
    constexpr unsigned char sourceBytes[] { 0x52, 0x49, 0x46, 0x46, 0x10, 0x20, 0x30, 0x40 };
    if (! source.replaceWithData(sourceBytes, sizeof(sourceBytes)))
        return fail(2, "could not create source fixture");

    c2paseq::MediaReference media;
    bool mediaWasAdded = false;
    if (const auto result = c2paseq::MediaLibrary::copySourceIntoProject(
            project, paths, source, media, &mediaWasAdded); result.failed())
        return fail(3, result.getErrorMessage());
    if (! mediaWasAdded)
        return fail(3, "first media registration was not reported as new");

    c2paseq::MediaReference duplicateMedia;
    bool duplicateWasAdded = true;
    if (const auto result = c2paseq::MediaLibrary::copySourceIntoProject(
            project, paths, source, duplicateMedia, &duplicateWasAdded); result.failed())
        return fail(3, result.getErrorMessage());
    if (duplicateWasAdded || duplicateMedia.id != media.id || project.media.size() != 1)
        return fail(3, "duplicate media was not deduplicated by SHA-256");
    project.media.front().provenance.status =
        c2paseq::ProvenanceStatus::presentWithValidationIssue;
    project.media.front().provenance.c2paPresent = true;
    project.media.front().provenance.assetIntact = true;
    project.media.front().provenance.activeManifest = "urn:c2pa:manifest:test";
    project.media.front().provenance.claimGenerator = "Fixture Generator 1.0";
    project.media.front().provenance.signer = "Fixture Signer";
    project.media.front().provenance.validationSummary = "External trust issue";

    c2paseq::ClipModel clip;
    clip.id = juce::Uuid().toString();
    clip.mediaId = media.id;
    clip.startSeconds = 4.0;
    clip.sourceOffsetSeconds = 1.25;
    clip.lengthSeconds = 8.5;
    clip.gainDb = -1.0;
    clip.fadeInSeconds = 0.1;
    clip.fadeOutSeconds = 0.2;
    track.clips.push_back(clip);
    project.tracks.push_back(track);

    c2paseq::TrackModel midiTrack;
    midiTrack.id = juce::Uuid().toString();
    midiTrack.name = "MIDI 1";
    midiTrack.type = c2paseq::TrackType::midi;
    c2paseq::MidiClipModel midiClip;
    midiClip.id = juce::Uuid().toString();
    midiClip.start.beats = 8.0;
    midiClip.length.beats = 4.0;
    for (const auto [pitch, start, duration, velocity] : {
             std::tuple { 60, 0.0, 1.0, 96 },
             std::tuple { 64, 1.0, 0.5, 104 },
             std::tuple { 67, 2.0, 1.5, 112 } })
    {
        c2paseq::MidiNote note;
        note.id = juce::Uuid().toString();
        note.noteNumber = pitch;
        note.start.beats = start;
        note.duration.beats = duration;
        note.velocity = velocity;
        midiClip.notes.push_back(std::move(note));
    }
    midiTrack.midiClips.push_back(midiClip);
    c2paseq::MidiClipModel emptyMidiClip;
    emptyMidiClip.id = juce::Uuid().toString();
    emptyMidiClip.start.beats = 12.0;
    emptyMidiClip.length.beats = 4.0;
    midiTrack.midiClips.push_back(emptyMidiClip);
    project.tracks.push_back(midiTrack);

    c2paseq::PluginState plugin;
    plugin.ownerId = track.id;
    plugin.pluginIdentifier = "VST3-example";
    plugin.name = "Example Gain";
    plugin.vendor = "Example Vendor";
    plugin.version = "1.2.3";
    plugin.format = "VST3";
    plugin.category = "Fx";
    plugin.fileOrIdentifier = "/Library/Audio/Plug-Ins/VST3/Example.vst3";
    plugin.uniqueId = 1234;
    plugin.deprecatedUid = 5678;
    plugin.bypassed = true;
    plugin.missing = true;
    plugin.stateBase64 = "AQID";
    project.plugins.push_back(plugin);
    project.provenance.ingredientManifestIds.push_back("urn:c2pa:ingredient:test");
    project.provenance.actionIds.push_back("c2pa.edited");

    if (! paths.arrangementEdit().replaceWithText("<EDIT bpm=\"127.5\" />"))
        return fail(4, "could not create arrangement fixture");
    if (const auto result = c2paseq::ProjectSerializer::save(project, paths); result.failed())
        return fail(5, result.getErrorMessage());

    const auto originalId = project.id;
    project = {};

    if (const auto result = c2paseq::ProjectSerializer::load(paths, project); result.failed())
        return fail(6, result.getErrorMessage());

    if (project.id != originalId || project.name != "Round Trip" || project.bpm != 127.5
        || project.timelinePixelsPerSecond != 144.0
        || project.timelineScrollSeconds != 9.5
        || project.loopStartSeconds != 4.0 || project.loopEndSeconds != 12.5
        || ! project.looping
        || project.createdAt.isEmpty() || project.modifiedAt.isEmpty()
        || project.applicationVersion.isEmpty() || project.tracks.size() != 2
        || project.media.size() != 1 || project.plugins.size() != 1)
        return fail(7, "project state did not round-trip");

    const auto& loadedTrack = project.tracks.front();
    if (loadedTrack.id != track.id || loadedTrack.name != track.name
        || loadedTrack.type != c2paseq::TrackType::audio
        || loadedTrack.gainDb != track.gainDb || loadedTrack.pan != track.pan
        || loadedTrack.muted != track.muted || loadedTrack.soloed != track.soloed
        || loadedTrack.clips.size() != 1)
        return fail(8, "track state did not round-trip");

    const auto& loadedClip = loadedTrack.clips.front();
    if (loadedClip.id != clip.id || loadedClip.mediaId != clip.mediaId
        || loadedClip.startSeconds != clip.startSeconds
        || loadedClip.sourceOffsetSeconds != clip.sourceOffsetSeconds
        || loadedClip.lengthSeconds != clip.lengthSeconds || loadedClip.gainDb != clip.gainDb
        || loadedClip.fadeInSeconds != clip.fadeInSeconds
        || loadedClip.fadeOutSeconds != clip.fadeOutSeconds)
        return fail(9, "clip state did not round-trip");

    const auto& loadedMidiTrack = project.tracks.back();
    if (loadedMidiTrack.id != midiTrack.id || loadedMidiTrack.name != midiTrack.name
        || loadedMidiTrack.type != c2paseq::TrackType::midi
        || ! loadedMidiTrack.clips.empty() || loadedMidiTrack.midiClips.size() != 2)
        return fail(9, "MIDI track state did not round-trip");
    const auto& loadedMidiClip = loadedMidiTrack.midiClips.front();
    if (loadedMidiClip.id != midiClip.id
        || loadedMidiClip.start.beats != midiClip.start.beats
        || loadedMidiClip.length.beats != midiClip.length.beats
        || loadedMidiClip.notes.size() != midiClip.notes.size())
        return fail(9, "MIDI clip identity or musical timing did not round-trip");
    for (std::size_t index = 0; index < midiClip.notes.size(); ++index)
    {
        const auto& expected = midiClip.notes[index];
        const auto& actual = loadedMidiClip.notes[index];
        if (actual.id != expected.id || actual.noteNumber != expected.noteNumber
            || actual.start.beats != expected.start.beats
            || actual.duration.beats != expected.duration.beats
            || actual.velocity != expected.velocity)
            return fail(9, "MIDI note values did not round-trip");
    }
    const auto& loadedEmptyMidiClip = loadedMidiTrack.midiClips.back();
    if (loadedEmptyMidiClip.id != emptyMidiClip.id
        || loadedEmptyMidiClip.start.beats != emptyMidiClip.start.beats
        || loadedEmptyMidiClip.length.beats != emptyMidiClip.length.beats
        || ! loadedEmptyMidiClip.notes.empty())
        return fail(9, "empty MIDI clip did not round-trip exactly");

    const c2paseq::MusicalTimeConverter at120Bpm(120.0);
    const c2paseq::MusicalTimeConverter at60Bpm(60.0);
    const auto storedBeatPosition = loadedMidiClip.notes.back().start;
    if (at120Bpm.toSeconds(storedBeatPosition) != 1.0
        || at60Bpm.toSeconds(storedBeatPosition) != 2.0
        || loadedMidiClip.notes.back().start.beats != storedBeatPosition.beats)
        return fail(9, "musical-time conversion changed stored MIDI position");

    if (project.plugins.front().ownerId != track.id
        || project.plugins.front().pluginIdentifier != plugin.pluginIdentifier
        || project.plugins.front().name != plugin.name
        || project.plugins.front().vendor != plugin.vendor
        || project.plugins.front().version != plugin.version
        || project.plugins.front().format != "VST3"
        || project.plugins.front().category != plugin.category
        || project.plugins.front().fileOrIdentifier != plugin.fileOrIdentifier
        || project.plugins.front().uniqueId != plugin.uniqueId
        || project.plugins.front().deprecatedUid != plugin.deprecatedUid
        || ! project.plugins.front().bypassed || ! project.plugins.front().missing
        || project.plugins.front().stateBase64 != "AQID"
        || project.provenance.ingredientManifestIds
            != std::vector<juce::String> { "urn:c2pa:ingredient:test" }
        || project.provenance.actionIds != std::vector<juce::String> { "c2pa.edited" })
        return fail(10, "plugin or provenance state did not round-trip");

    const auto copiedMedia = paths.root().getChildFile(project.media.front().relativePath);
    if (! copiedMedia.existsAsFile()
        || copiedMedia.getSize() != source.getSize()
        || juce::SHA256(copiedMedia) != juce::SHA256(source)
        || project.media.front().sha256 != juce::SHA256(source).toHexString())
        return fail(11, "media copy was not byte-for-byte identical");
    const auto& loadedProvenance = project.media.front().provenance;
    if (! loadedProvenance.c2paPresent || ! loadedProvenance.assetIntact
        || loadedProvenance.status
            != c2paseq::ProvenanceStatus::presentWithValidationIssue
        || loadedProvenance.activeManifest != "urn:c2pa:manifest:test"
        || loadedProvenance.claimGenerator != "Fixture Generator 1.0"
        || loadedProvenance.signer != "Fixture Signer"
        || loadedProvenance.validationSummary != "External trust issue")
        return fail(11, "media provenance summary did not round-trip");

    if (! paths.projectJson().existsAsFile() || ! paths.provenanceJson().existsAsFile()
        || ! paths.arrangementEdit().existsAsFile() || ! paths.mediaDirectory().isDirectory())
        return fail(12, "required project bundle structure is incomplete");

    const c2paseq::ProjectPaths legacyPaths(
        temporary.directory.getChildFile("Legacy Audio.c2paseq"));
    auto legacySource = c2paseq::Project::create("Legacy Audio");
    c2paseq::TrackModel legacyTrack;
    legacyTrack.id = juce::Uuid().toString();
    legacyTrack.name = "Legacy Stem";
    legacySource.tracks.push_back(legacyTrack);
    if (c2paseq::ProjectSerializer::save(legacySource, legacyPaths).failed()
        || ! legacyPaths.arrangementEdit().replaceWithText("<EDIT bpm=\"120\" />"))
        return fail(13, "could not create legacy migration fixture");

    juce::var legacyDocument;
    if (juce::JSON::parse(legacyPaths.projectJson().loadFileAsString(), legacyDocument).failed())
        return fail(13, "could not parse legacy migration fixture");
    auto* legacyRoot = legacyDocument.getDynamicObject();
    auto* legacyTracks = legacyRoot != nullptr
        ? legacyRoot->getProperty("tracks").getArray() : nullptr;
    if (legacyRoot == nullptr || legacyTracks == nullptr || legacyTracks->size() != 1)
        return fail(13, "legacy migration fixture structure was invalid");
    legacyRoot->setProperty("schemaVersion", 1);
    auto* legacyTrackObject = legacyTracks->getReference(0).getDynamicObject();
    if (legacyTrackObject == nullptr)
        return fail(13, "legacy track fixture was invalid");
    legacyTrackObject->removeProperty("type");
    legacyTrackObject->removeProperty("midiClips");
    if (! legacyPaths.projectJson().replaceWithText(
            juce::JSON::toString(legacyDocument, true)))
        return fail(13, "could not write legacy migration fixture");

    c2paseq::Project migratedLegacy;
    if (const auto result = c2paseq::ProjectSerializer::load(legacyPaths, migratedLegacy);
        result.failed())
        return fail(14, "legacy audio project did not load: " + result.getErrorMessage());
    if (migratedLegacy.tracks.size() != 1
        || migratedLegacy.tracks.front().type != c2paseq::TrackType::audio
        || ! migratedLegacy.tracks.front().midiClips.empty())
        return fail(14, "legacy track without type did not migrate as Audio");

    std::cout << "project model: audio/MIDI round-trip, musical-time conversion, legacy "
                 "migration, and byte-preserving media passed\n";
    return 0;
}
