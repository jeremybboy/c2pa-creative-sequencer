#include "ProjectSerializer.h"

#include <cmath>

namespace c2paseq
{
namespace
{
constexpr int schemaVersion = 1;

juce::var makeClip(const ClipModel& clip)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("id", clip.id);
    object->setProperty("mediaId", clip.mediaId);
    object->setProperty("startSeconds", clip.startSeconds);
    object->setProperty("sourceOffsetSeconds", clip.sourceOffsetSeconds);
    object->setProperty("lengthSeconds", clip.lengthSeconds);
    object->setProperty("gainDb", clip.gainDb);
    object->setProperty("fadeInSeconds", clip.fadeInSeconds);
    object->setProperty("fadeOutSeconds", clip.fadeOutSeconds);
    return object.release();
}

juce::var makeTrack(const TrackModel& track)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("id", track.id);
    object->setProperty("name", track.name);
    object->setProperty("gainDb", track.gainDb);
    object->setProperty("pan", track.pan);
    object->setProperty("muted", track.muted);
    object->setProperty("soloed", track.soloed);

    juce::Array<juce::var> clips;
    for (const auto& clip : track.clips)
        clips.add(makeClip(clip));
    object->setProperty("clips", clips);
    return object.release();
}

juce::var makeMedia(const MediaReference& media)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("id", media.id);
    object->setProperty("originalFileName", media.originalFileName);
    object->setProperty("relativePath", media.relativePath);
    object->setProperty("sha256", media.sha256);
    object->setProperty("byteSize", media.byteSize);
    return object.release();
}

juce::var makePlugin(const PluginState& plugin)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("ownerId", plugin.ownerId);
    object->setProperty("pluginIdentifier", plugin.pluginIdentifier);
    object->setProperty("stateBase64", plugin.stateBase64);
    return object.release();
}

juce::var makeProjectDocument(const Project& project)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("schemaVersion", schemaVersion);
    object->setProperty("id", project.id);
    object->setProperty("name", project.name);
    object->setProperty("createdAt", project.createdAt);
    object->setProperty("modifiedAt", project.modifiedAt);
    object->setProperty("bpm", project.bpm);
    object->setProperty("applicationVersion", project.applicationVersion);
    object->setProperty("arrangementFile", "arrangement.tracktionedit");
    object->setProperty("provenanceFile", "provenance.json");

    juce::Array<juce::var> tracks;
    for (const auto& track : project.tracks)
        tracks.add(makeTrack(track));
    object->setProperty("tracks", tracks);

    juce::Array<juce::var> media;
    for (const auto& item : project.media)
        media.add(makeMedia(item));
    object->setProperty("media", media);

    juce::Array<juce::var> plugins;
    for (const auto& plugin : project.plugins)
        plugins.add(makePlugin(plugin));
    object->setProperty("plugins", plugins);
    return object.release();
}

juce::var makeProvenanceDocument(const Project& project)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("schemaVersion", schemaVersion);
    object->setProperty("projectId", project.id);

    juce::Array<juce::var> ingredientIds;
    for (const auto& id : project.provenance.ingredientManifestIds)
        ingredientIds.add(id);
    object->setProperty("ingredientManifestIds", ingredientIds);

    juce::Array<juce::var> actionIds;
    for (const auto& id : project.provenance.actionIds)
        actionIds.add(id);
    object->setProperty("actionIds", actionIds);
    return object.release();
}

juce::Result writeJsonAtomically(const juce::File& target, const juce::var& document)
{
    juce::TemporaryFile temporary(target);
    if (! temporary.getFile().replaceWithText(juce::JSON::toString(document, true)))
        return juce::Result::fail("Could not write " + target.getFileName());

    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not replace " + target.getFileName());

    return juce::Result::ok();
}

juce::Result readJson(const juce::File& file, juce::var& document)
{
    if (! file.existsAsFile())
        return juce::Result::fail("Missing " + file.getFileName());

    auto stream = file.createInputStream();
    if (stream == nullptr)
        return juce::Result::fail("Could not read " + file.getFileName());

    return juce::JSON::parse(stream->readEntireStreamAsString(), document);
}

juce::Result requireObject(const juce::var& value, juce::DynamicObject*& object, juce::String context)
{
    object = value.getDynamicObject();
    if (object == nullptr)
        return juce::Result::fail(context + " must be a JSON object");
    return juce::Result::ok();
}

juce::Result requireString(juce::DynamicObject& object, const juce::Identifier& key,
                           juce::String& output)
{
    const auto value = object.getProperty(key);
    if (! value.isString() || value.toString().isEmpty())
        return juce::Result::fail(key.toString() + " must be a non-empty string");
    output = value.toString();
    return juce::Result::ok();
}

juce::Result requireNumber(juce::DynamicObject& object, const juce::Identifier& key,
                           double& output)
{
    const auto value = object.getProperty(key);
    if (! (value.isDouble() || value.isInt() || value.isInt64()))
        return juce::Result::fail(key.toString() + " must be a number");
    output = static_cast<double>(value);
    if (! std::isfinite(output))
        return juce::Result::fail(key.toString() + " must be finite");
    return juce::Result::ok();
}

juce::Result requireArray(juce::DynamicObject& object, const juce::Identifier& key,
                          juce::Array<juce::var>*& output)
{
    output = object.getProperty(key).getArray();
    if (output == nullptr)
        return juce::Result::fail(key.toString() + " must be an array");
    return juce::Result::ok();
}

juce::Result requireSchemaVersion(juce::DynamicObject& object)
{
    const auto value = object.getProperty("schemaVersion");
    if (! value.isInt() || static_cast<int>(value) != schemaVersion)
        return juce::Result::fail("Unsupported project schema version");
    return juce::Result::ok();
}

juce::Result parseClip(const juce::var& value, ClipModel& clip)
{
    juce::DynamicObject* object = nullptr;
    if (auto result = requireObject(value, object, "clip"); result.failed())
        return result;
    if (auto result = requireString(*object, "id", clip.id); result.failed()) return result;
    if (auto result = requireString(*object, "mediaId", clip.mediaId); result.failed()) return result;
    if (auto result = requireNumber(*object, "startSeconds", clip.startSeconds); result.failed()) return result;
    if (auto result = requireNumber(*object, "sourceOffsetSeconds", clip.sourceOffsetSeconds); result.failed()) return result;
    if (auto result = requireNumber(*object, "lengthSeconds", clip.lengthSeconds); result.failed()) return result;
    if (auto result = requireNumber(*object, "gainDb", clip.gainDb); result.failed()) return result;
    if (auto result = requireNumber(*object, "fadeInSeconds", clip.fadeInSeconds); result.failed()) return result;
    return requireNumber(*object, "fadeOutSeconds", clip.fadeOutSeconds);
}

juce::Result parseTrack(const juce::var& value, TrackModel& track)
{
    juce::DynamicObject* object = nullptr;
    if (auto result = requireObject(value, object, "track"); result.failed()) return result;
    if (auto result = requireString(*object, "id", track.id); result.failed()) return result;
    if (auto result = requireString(*object, "name", track.name); result.failed()) return result;
    if (auto result = requireNumber(*object, "gainDb", track.gainDb); result.failed()) return result;
    if (auto result = requireNumber(*object, "pan", track.pan); result.failed()) return result;

    const auto muted = object->getProperty("muted");
    const auto soloed = object->getProperty("soloed");
    if (! muted.isBool() || ! soloed.isBool())
        return juce::Result::fail("track mute and solo values must be booleans");
    track.muted = static_cast<bool>(muted);
    track.soloed = static_cast<bool>(soloed);

    juce::Array<juce::var>* clips = nullptr;
    if (auto result = requireArray(*object, "clips", clips); result.failed()) return result;
    for (const auto& clipValue : *clips)
    {
        ClipModel clip;
        if (auto result = parseClip(clipValue, clip); result.failed()) return result;
        track.clips.push_back(std::move(clip));
    }
    return juce::Result::ok();
}

juce::Result parseMedia(const juce::var& value, const ProjectPaths& paths, MediaReference& media)
{
    juce::DynamicObject* object = nullptr;
    if (auto result = requireObject(value, object, "media reference"); result.failed()) return result;
    if (auto result = requireString(*object, "id", media.id); result.failed()) return result;
    if (auto result = requireString(*object, "originalFileName", media.originalFileName); result.failed()) return result;
    if (auto result = requireString(*object, "relativePath", media.relativePath); result.failed()) return result;
    if (auto result = requireString(*object, "sha256", media.sha256); result.failed()) return result;

    const auto byteSize = object->getProperty("byteSize");
    if (! (byteSize.isInt() || byteSize.isInt64()) || static_cast<juce::int64>(byteSize) < 0)
        return juce::Result::fail("byteSize must be a non-negative integer");
    media.byteSize = static_cast<juce::int64>(byteSize);

    const auto mediaFile = paths.root().getChildFile(media.relativePath);
    if (! mediaFile.isAChildOf(paths.mediaDirectory()) || media.sha256.length() != 64)
        return juce::Result::fail("Invalid media reference");
    return juce::Result::ok();
}

juce::Result parsePlugin(const juce::var& value, PluginState& plugin)
{
    juce::DynamicObject* object = nullptr;
    if (auto result = requireObject(value, object, "plugin state"); result.failed()) return result;
    if (auto result = requireString(*object, "ownerId", plugin.ownerId); result.failed()) return result;
    if (auto result = requireString(*object, "pluginIdentifier", plugin.pluginIdentifier); result.failed()) return result;
    const auto state = object->getProperty("stateBase64");
    if (! state.isString())
        return juce::Result::fail("stateBase64 must be a string");
    plugin.stateBase64 = state.toString();
    return juce::Result::ok();
}

juce::Result parseStringArray(juce::DynamicObject& object, const juce::Identifier& key,
                              std::vector<juce::String>& output)
{
    juce::Array<juce::var>* values = nullptr;
    if (auto result = requireArray(object, key, values); result.failed()) return result;
    for (const auto& value : *values)
    {
        if (! value.isString() || value.toString().isEmpty())
            return juce::Result::fail(key.toString() + " entries must be non-empty strings");
        output.push_back(value.toString());
    }
    return juce::Result::ok();
}
}

juce::Result ProjectSerializer::save(const Project& project, const ProjectPaths& paths)
{
    if (project.id.isEmpty() || project.name.isEmpty())
        return juce::Result::fail("Project identity is incomplete");
    if (project.bpm < 40.0 || project.bpm > 240.0)
        return juce::Result::fail("Project BPM must be between 40 and 240");
    if (auto result = paths.createDirectories(); result.failed())
        return result;
    if (auto result = writeJsonAtomically(paths.projectJson(), makeProjectDocument(project)); result.failed())
        return result;
    return writeJsonAtomically(paths.provenanceJson(), makeProvenanceDocument(project));
}

juce::Result ProjectSerializer::load(const ProjectPaths& paths, Project& project)
{
    if (! ProjectPaths::hasProjectExtension(paths.root()) || ! paths.root().isDirectory())
        return juce::Result::fail("Not a .c2paseq project folder");
    if (! paths.arrangementEdit().existsAsFile())
        return juce::Result::fail("Missing arrangement.tracktionedit");

    juce::var projectDocument;
    if (auto result = readJson(paths.projectJson(), projectDocument); result.failed()) return result;
    juce::DynamicObject* root = nullptr;
    if (auto result = requireObject(projectDocument, root, "project.json"); result.failed()) return result;
    if (auto result = requireSchemaVersion(*root); result.failed()) return result;

    Project loaded;
    if (auto result = requireString(*root, "id", loaded.id); result.failed()) return result;
    if (auto result = requireString(*root, "name", loaded.name); result.failed()) return result;
    if (auto result = requireString(*root, "createdAt", loaded.createdAt); result.failed()) return result;
    if (auto result = requireString(*root, "modifiedAt", loaded.modifiedAt); result.failed()) return result;
    if (auto result = requireNumber(*root, "bpm", loaded.bpm); result.failed()) return result;
    if (loaded.bpm < 40.0 || loaded.bpm > 240.0)
        return juce::Result::fail("Project BPM must be between 40 and 240");
    if (auto result = requireString(*root, "applicationVersion", loaded.applicationVersion); result.failed()) return result;
    if (root->getProperty("arrangementFile").toString() != "arrangement.tracktionedit"
        || root->getProperty("provenanceFile").toString() != "provenance.json")
        return juce::Result::fail("Project companion file names are invalid");

    juce::Array<juce::var>* tracks = nullptr;
    if (auto result = requireArray(*root, "tracks", tracks); result.failed()) return result;
    for (const auto& value : *tracks)
    {
        TrackModel track;
        if (auto result = parseTrack(value, track); result.failed()) return result;
        loaded.tracks.push_back(std::move(track));
    }

    juce::Array<juce::var>* media = nullptr;
    if (auto result = requireArray(*root, "media", media); result.failed()) return result;
    for (const auto& value : *media)
    {
        MediaReference reference;
        if (auto result = parseMedia(value, paths, reference); result.failed()) return result;
        loaded.media.push_back(std::move(reference));
    }

    juce::Array<juce::var>* plugins = nullptr;
    if (auto result = requireArray(*root, "plugins", plugins); result.failed()) return result;
    for (const auto& value : *plugins)
    {
        PluginState plugin;
        if (auto result = parsePlugin(value, plugin); result.failed()) return result;
        loaded.plugins.push_back(std::move(plugin));
    }

    juce::var provenanceDocument;
    if (auto result = readJson(paths.provenanceJson(), provenanceDocument); result.failed()) return result;
    juce::DynamicObject* provenance = nullptr;
    if (auto result = requireObject(provenanceDocument, provenance, "provenance.json"); result.failed()) return result;
    if (auto result = requireSchemaVersion(*provenance); result.failed()) return result;
    if (provenance->getProperty("projectId").toString() != loaded.id)
        return juce::Result::fail("Provenance document does not match the project");
    if (auto result = parseStringArray(*provenance, "ingredientManifestIds",
                                       loaded.provenance.ingredientManifestIds); result.failed()) return result;
    if (auto result = parseStringArray(*provenance, "actionIds",
                                       loaded.provenance.actionIds); result.failed()) return result;

    project = std::move(loaded);
    return juce::Result::ok();
}
}
