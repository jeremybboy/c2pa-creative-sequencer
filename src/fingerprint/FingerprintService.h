#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <string_view>

namespace c2paseq
{
inline constexpr std::string_view audfprintAlgorithm =
    "io.github.jeremybboy.audfprint.1";
inline constexpr std::string_view audfprintCommit =
    "cb03ba99feafd41b8874307f0f4e808a6ce34362";

struct FingerprintRegistration
{
    juce::File artifact;
    juce::String valueHex;
    juce::String engineVersion;
    int hashCount = 0;
    double durationSeconds = 0.0;
    double elapsedSeconds = 0.0;
};

class FingerprintService
{
public:
    virtual ~FingerprintService() = default;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual juce::String statusDescription() const = 0;
    [[nodiscard]] virtual juce::Result compute(
        const juce::File& input, const juce::File& outputArtifact,
        FingerprintRegistration&, const std::function<bool()>& shouldCancel = {}) = 0;
};
}
