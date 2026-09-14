#include "SigningProvider.h"

#include <sys/stat.h>

namespace c2paseq
{
namespace
{
juce::Result applyPermissions(const juce::File& file, mode_t permissions,
                              const juce::String& description)
{
    if (::chmod(file.getFullPathName().toRawUTF8(), permissions) != 0)
        return juce::Result::fail("Could not secure " + description);
    return juce::Result::ok();
}
}

ConformanceTestSigningProvider::ConformanceTestSigningProvider(
    juce::File directory, bool allowEnvironmentOverride)
    : configurationDirectory(std::move(directory)),
      environmentOverrideAllowed(allowEnvironmentOverride)
{
}

juce::File ConformanceTestSigningProvider::defaultConfigurationDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("Signing");
}

bool ConformanceTestSigningProvider::isDeveloperOverrideActive() const
{
    return environmentOverrideAllowed
        && juce::SystemStats::getEnvironmentVariable(environmentVariable, {}).isNotEmpty();
}

juce::File ConformanceTestSigningProvider::storedCredentialFile() const
{
    return configurationDirectory.getChildFile("signing-bundle.pem");
}

juce::File ConformanceTestSigningProvider::credentialFile() const
{
    if (isDeveloperOverrideActive())
        return juce::File(juce::SystemStats::getEnvironmentVariable(environmentVariable, {}));
    const auto stored = storedCredentialFile();
    return stored.existsAsFile() ? stored : juce::File();
}

juce::String ConformanceTestSigningProvider::configurationError() const
{
    const auto file = credentialFile();
    if (file == juce::File())
        return "C2PA signing credential has not been configured.";
    if (! file.existsAsFile())
        return "Configured C2PA signing credential cannot be read.";
    return {};
}

juce::String ConformanceTestSigningProvider::statusDescription() const
{
    if (const auto error = configurationError(); error.isNotEmpty())
        return error;
    return isDeveloperOverrideActive()
        ? "Configured (developer environment override)"
        : "Configured (private machine-local Application Support storage)";
}

juce::Result ConformanceTestSigningProvider::installCredential(const juce::File& source)
{
    if (! source.existsAsFile())
        return juce::Result::fail("Selected signing credential is not readable.");
    if (auto result = configurationDirectory.createDirectory(); result.failed())
        return juce::Result::fail("Could not create private signing configuration directory.");
    if (auto result = applyPermissions(configurationDirectory, 0700,
                                       "signing configuration directory"); result.failed())
        return result;

    const auto destination = storedCredentialFile();
    juce::TemporaryFile temporary(destination);
    if (! source.copyFileTo(temporary.getFile()))
        return juce::Result::fail("Could not copy signing credential to private storage.");
    if (! source.hasIdenticalContentTo(temporary.getFile()))
        return juce::Result::fail("Stored signing credential did not match the selected file.");
    if (auto result = applyPermissions(temporary.getFile(), 0600,
                                       "stored signing credential"); result.failed())
        return result;
    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not commit signing credential to private storage.");
    return applyPermissions(destination, 0600, "stored signing credential");
}

juce::Result ConformanceTestSigningProvider::removeCredential()
{
    const auto stored = storedCredentialFile();
    if (! stored.exists())
        return juce::Result::ok();
    if (! stored.deleteFile())
        return juce::Result::fail("Could not remove the machine-local signing credential.");
    return juce::Result::ok();
}
}
