#pragma once

#include "provenance/ProvenanceModel.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace c2paseq
{
struct ClipModel
{
    juce::String id;
    juce::String mediaId;
    double startSeconds = 0.0;
    double sourceOffsetSeconds = 0.0;
    double lengthSeconds = 0.0;
    double gainDb = 0.0;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
};

struct TrackModel
{
    juce::String id;
    juce::String name;
    double gainDb = 0.0;
    double pan = 0.0;
    bool muted = false;
    bool soloed = false;
    std::vector<ClipModel> clips;
};

struct MediaReference
{
    juce::String id;
    juce::String originalFileName;
    juce::String relativePath;
    juce::String sha256;
    std::int64_t byteSize = 0;
    IngredientInfo provenance;
};

struct PluginState
{
    juce::String ownerId;
    juce::String pluginIdentifier;
    juce::String name;
    juce::String vendor;
    juce::String version;
    juce::String format;
    juce::String category;
    juce::String fileOrIdentifier;
    int uniqueId = 0;
    int deprecatedUid = 0;
    bool isInstrument = false;
    bool bypassed = false;
    bool missing = false;
    juce::String stateBase64;
};

struct ProvenanceState
{
    std::vector<juce::String> ingredientManifestIds;
    std::vector<juce::String> actionIds;
};

struct Project
{
    static Project create(juce::String name);

    juce::String id;
    juce::String name;
    juce::String createdAt;
    juce::String modifiedAt;
    double bpm = 120.0;
    double timelinePixelsPerSecond = 96.0;
    double timelineScrollSeconds = 0.0;
    std::vector<TrackModel> tracks;
    std::vector<MediaReference> media;
    std::vector<PluginState> plugins;
    ProvenanceState provenance;
    juce::String applicationVersion;
};
}
