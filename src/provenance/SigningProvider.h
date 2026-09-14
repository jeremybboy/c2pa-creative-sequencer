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
    [[nodiscard]] virtual juce::String statusDescription() const = 0;
    [[nodiscard]] virtual bool isDeveloperOverrideActive() const = 0;
    [[nodiscard]] virtual juce::Result installCredential(const juce::File&) = 0;
    [[nodiscard]] virtual juce::Result removeCredential() = 0;
};

class ConformanceTestSigningProvider final : public SigningProvider
{
public:
    static constexpr const char* environmentVariable = "C2PASEQ_SIGNING_BUNDLE_PEM";

    explicit ConformanceTestSigningProvider(
        juce::File configurationDirectory = defaultConfigurationDirectory(),
        bool allowEnvironmentOverride = true);

    [[nodiscard]] static juce::File defaultConfigurationDirectory();

    [[nodiscard]] juce::File credentialFile() const override;
    [[nodiscard]] juce::String configurationError() const override;
    [[nodiscard]] juce::String statusDescription() const override;
    [[nodiscard]] bool isDeveloperOverrideActive() const override;
    [[nodiscard]] juce::Result installCredential(const juce::File&) override;
    [[nodiscard]] juce::Result removeCredential() override;
    [[nodiscard]] juce::File storedCredentialFile() const;

private:
    juce::File configurationDirectory;
    bool environmentOverrideAllowed = true;
};
}
