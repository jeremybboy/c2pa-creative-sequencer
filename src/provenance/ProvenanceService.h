#pragma once

#include "ProvenanceModel.h"
#include "SigningProvider.h"

#include <memory>

namespace c2paseq
{
class ProvenanceService final
{
public:
    ProvenanceService();

    [[nodiscard]] IngredientInfo inspect(const juce::File& asset) const;
    [[nodiscard]] bool signingConfigured() const;
    [[nodiscard]] juce::String signingConfigurationError() const;
    [[nodiscard]] juce::Result signWav(const juce::File& unsignedWav,
                                       const juce::File& destination,
                                       const std::vector<ContributingIngredient>& ingredients,
                                       const juce::String& outputTitle,
                                       IngredientInfo& validation) const;

private:
    std::unique_ptr<SigningProvider> signingProvider;
};
}
