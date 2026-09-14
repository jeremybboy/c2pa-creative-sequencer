#include "SigningProvider.h"

namespace c2paseq
{
juce::File ConformanceTestSigningProvider::credentialFile() const
{
    const auto configured = juce::SystemStats::getEnvironmentVariable(environmentVariable, {});
    return configured.isNotEmpty() ? juce::File(configured) : juce::File();
}

juce::String ConformanceTestSigningProvider::configurationError() const
{
    const auto file = credentialFile();
    if (file == juce::File())
        return juce::String(environmentVariable) + " is not set";
    if (! file.existsAsFile())
        return juce::String(environmentVariable) + " does not name a readable PEM file";
    return {};
}
}
