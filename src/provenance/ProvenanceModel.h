#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>
#include <optional>
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

[[nodiscard]] juce::String provenanceStatusId(ProvenanceStatus status);
[[nodiscard]] juce::String provenanceStatusLabel(ProvenanceStatus status);
[[nodiscard]] ProvenanceStatus provenanceStatusFromId(const juce::String& id);

struct IngredientInfo
{
    ProvenanceStatus status = ProvenanceStatus::noCredentials;
    bool c2paPresent = false;
    bool assetIntact = false;
    juce::String activeManifest;
    juce::String claimGenerator;
    juce::String signer;
    juce::String validationSummary;
    juce::String rawManifestJson;
    std::vector<juce::String> validationIssues;
};

enum class SoftBindingType
{
    watermark,
    fingerprint
};

struct SoftBindingScope
{
    std::uint64_t startMilliseconds = 0;
    std::uint64_t endMilliseconds = 0;
};

struct SoftBindingClaim
{
    juce::String algorithm;
    SoftBindingType type = SoftBindingType::fingerprint;
    std::vector<std::uint8_t> value;
    std::optional<SoftBindingScope> scope;
};

[[nodiscard]] SoftBindingClaim makeAudioWMarkClaim(
    const SoftBindingPayload&, std::uint64_t endMilliseconds);

struct ContributingIngredient
{
    juce::String mediaId;
    juce::String title;
    juce::String sha256;
    juce::File file;
    IngredientInfo provenance;
};
}
