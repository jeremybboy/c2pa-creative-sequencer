#pragma once

#include "ProvenanceModel.h"
#include "SigningProvider.h"

#include <memory>
#include <optional>
#include <vector>

namespace c2paseq
{
class ProvenanceService final
{
public:
    explicit ProvenanceService(std::unique_ptr<SigningProvider> provider = {});

    [[nodiscard]] IngredientInfo inspect(const juce::File& asset) const;
    [[nodiscard]] bool signingConfigured() const;
    [[nodiscard]] juce::String signingConfigurationError() const;
    [[nodiscard]] juce::String signingCredentialStatus() const;
    [[nodiscard]] juce::Result validateSigningCredential(const juce::File&) const;
    [[nodiscard]] juce::Result configureSigningCredential(const juce::File&);
    [[nodiscard]] juce::Result removeSigningCredential();
    [[nodiscard]] juce::Result signWav(const juce::File& unsignedWav,
                                       const juce::File& destination,
                                       const std::vector<ContributingIngredient>& ingredients,
                                       const juce::String& outputTitle,
                                       IngredientInfo& validation,
                                       const std::optional<SoftBindingClaim>& softBinding = {},
                                       std::vector<std::uint8_t>* manifestStore = nullptr) const;
    [[nodiscard]] IngredientInfo inspectRecoveredManifest(
        const juce::File& asset,
        const std::vector<std::uint8_t>& manifestStore) const;
    [[nodiscard]] static bool hasMatchingSoftBinding(
        const IngredientInfo&, const SoftBindingPayload&);

private:
    std::unique_ptr<SigningProvider> signingProvider;
};
}
