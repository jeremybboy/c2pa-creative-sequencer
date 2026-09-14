#include "project/MediaLibrary.h"
#include "project/ProjectSerializer.h"

#include <juce_cryptography/juce_cryptography.h>

#include <iostream>

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

    c2paseq::TrackModel track;
    track.id = juce::Uuid().toString();
    track.name = "Stem 1";
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

    project.plugins.push_back({ track.id, "example.vst3", "AQID" });
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
        || project.createdAt.isEmpty() || project.modifiedAt.isEmpty()
        || project.applicationVersion.isEmpty() || project.tracks.size() != 1
        || project.media.size() != 1 || project.plugins.size() != 1)
        return fail(7, "project state did not round-trip");

    const auto& loadedTrack = project.tracks.front();
    if (loadedTrack.id != track.id || loadedTrack.name != track.name
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

    if (project.plugins.front().ownerId != track.id
        || project.plugins.front().pluginIdentifier != "example.vst3"
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

    std::cout << "project model: save, close, reopen, and byte-preserving media passed\n";
    return 0;
}
