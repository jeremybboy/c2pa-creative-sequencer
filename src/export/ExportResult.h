#pragma once

#include "provenance/ProvenanceModel.h"

#include <functional>

namespace c2paseq
{
enum class ExportStage
{
    complete,
    planning,
    audioRender,
    watermarkEmbedding,
    signingConfiguration,
    creatingContentCredentials,
    signingAndEmbedding,
    publicationOutbox,
    finalValidation,
    fileCommit,
    cancelled
};

using ExportProgressCallback = std::function<void(ExportStage)>;
using ExportCancellationCheck = std::function<bool()>;

struct ExportResult
{
    ExportStage stage = ExportStage::planning;
    juce::Result result = juce::Result::fail("Export has not run");
    juce::File outputFile;
    bool audioRendered = false;
    bool credentialsAttached = false;
    bool credentialsValidated = false;
    bool externallyTrusted = false;
    bool softBindingEnabled = false;
    bool audioPropertiesVerified = false;
    juce::String softBindingPayloadHex;
    juce::String softBindingManifestId;
    juce::File publicationPackage;
    double renderSeconds = 0.0;
    double watermarkEmbedSeconds = 0.0;
    double signingAndValidationSeconds = 0.0;
    double publicationSeconds = 0.0;
    double totalSeconds = 0.0;
    IngredientInfo outputProvenance;
    std::vector<ContributingIngredient> ingredients;
};
}
