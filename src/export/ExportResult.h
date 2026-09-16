#pragma once

#include "provenance/ProvenanceModel.h"

namespace c2paseq
{
enum class ExportStage
{
    complete,
    planning,
    audioRender,
    watermarkEmbedding,
    watermarkVerification,
    signingConfiguration,
    signingAndEmbedding,
    recoveryStorePersistence,
    finalValidation,
    fileCommit
};

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
    bool watermarkVerified = false;
    juce::String softBindingPayloadHex;
    juce::String softBindingManifestId;
    double watermarkSnrDb = 0.0;
    IngredientInfo outputProvenance;
    std::vector<ContributingIngredient> ingredients;
};
}
