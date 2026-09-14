#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
class SigningProvider
{
public:
    virtual ~SigningProvider() = default;
    [[nodiscard]] virtual juce::File credentialFile() const = 0;
    [[nodiscard]] virtual juce::String configurationError() const = 0;
};

class ConformanceTestSigningProvider final : public SigningProvider
{
public:
    static constexpr const char* environmentVariable = "C2PASEQ_SIGNING_BUNDLE_PEM";

    [[nodiscard]] juce::File credentialFile() const override;
    [[nodiscard]] juce::String configurationError() const override;
};
}
