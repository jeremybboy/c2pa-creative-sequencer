#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "watermark/SoftBindingPayload.h"

namespace c2paseq
{
enum class ProvenanceStatus
{
    valid,
    presentWithValidationIssue,
    noCredentials,
    unableToValidate
};

enum class ProvenanceRetrievalMode
{
    none,
    embedded,
    recoveredSoftBinding
};

[[nodiscard]] juce::String provenanceStatusId(ProvenanceStatus status);
[[nodiscard]] juce::String provenanceStatusLabel(ProvenanceStatus status);
[[nodiscard]] ProvenanceStatus provenanceStatusFromId(const juce::String& id);

struct IngredientInfo
{
    ProvenanceStatus status = ProvenanceStatus::noCredentials;
    bool c2paPresent = false;
    bool assetIntact = false;
    ProvenanceRetrievalMode retrievalMode = ProvenanceRetrievalMode::none;
    juce::String activeManifest;
    juce::String claimGenerator;
    juce::String signer;
    juce::String validationSummary;
    juce::String rawManifestJson;
    std::vector<juce::String> validationIssues;
    juce::String softBindingAlgorithm;
    juce::String softBindingPayloadHex;
};

struct SoftBindingClaim
{
    static constexpr const char* algorithm = "com.microsoft.wavmark.1";
    SoftBindingPayload payload;
    std::uint64_t startMilliseconds = 0;
    std::uint64_t endMilliseconds = 0;
};

struct ContributingIngredient
{
    juce::String mediaId;
    juce::String title;
    juce::String sha256;
    juce::File file;
    IngredientInfo provenance;
};
}
