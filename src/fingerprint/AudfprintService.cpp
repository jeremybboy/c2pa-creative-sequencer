#include "AudfprintService.h"

#include <juce_cryptography/juce_cryptography.h>

namespace c2paseq
{
namespace
{
constexpr double timeoutSeconds = 180.0;
}

AudfprintService::AudfprintService(juce::File executableFile)
    : executable(std::move(executableFile))
{
}

juce::File AudfprintService::defaultExecutable()
{
    const auto override = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_AUDFPRINT_EXECUTABLE", {});
    if (override.isNotEmpty())
        return juce::File(override);
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("Fingerprint/bin/c2paseq-audfprint");
}

bool AudfprintService::isAvailable() const
{
    return executable.existsAsFile();
}

juce::String AudfprintService::statusDescription() const
{
    return isAvailable()
        ? "Ready (external audfprint runtime)"
        : "Setup required: run scripts/setup_audfprint.sh";
}

juce::Result AudfprintService::compute(const juce::File& input,
                                       const juce::File& output,
                                       FingerprintRegistration& registration,
                                       const std::function<bool()>& shouldCancel)
{
    if (! isAvailable()) return juce::Result::fail(statusDescription());
    if (! input.existsAsFile())
        return juce::Result::fail("Fingerprint input audio is missing");
    output.deleteFile();
    juce::ChildProcess process;
    const juce::StringArray command { executable.getFullPathName(), "fingerprint",
                                      input.getFullPathName(), output.getFullPathName() };
    const auto started = juce::Time::getMillisecondCounterHiRes();
    if (! process.start(command))
        return juce::Result::fail("Could not start the external audfprint helper");
    while (process.isRunning())
    {
        if (shouldCancel && shouldCancel())
        {
            process.kill();
            output.deleteFile();
            return juce::Result::fail("Export cancelled during fingerprint computation");
        }
        if ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0 > timeoutSeconds)
        {
            process.kill();
            output.deleteFile();
            return juce::Result::fail("Audio fingerprinting exceeded the 180-second timeout");
        }
        juce::Thread::sleep(20);
    }
    const auto responseText = process.readAllProcessOutput().trim();
    juce::var response;
    if (process.getExitCode() != 0 || juce::JSON::parse(responseText, response).failed()
        || ! response.isObject() || ! output.existsAsFile())
    {
        output.deleteFile();
        return juce::Result::fail("audfprint fingerprinting failed: " + responseText);
    }
    juce::FileInputStream stream(output);
    if (! stream.openedOk())
        return juce::Result::fail("Could not read the audfprint registration artifact");
    const auto digest = juce::SHA256(stream);
    registration.artifact = output;
    registration.valueHex = digest.toHexString();
    registration.engineVersion = response["engineVersion"].toString();
    registration.hashCount = static_cast<int>(response["hashCount"]);
    registration.durationSeconds = static_cast<double>(response["durationSeconds"]);
    registration.elapsedSeconds =
        (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    if (registration.hashCount <= 0)
        return juce::Result::fail("audfprint produced no landmark hashes");
    return juce::Result::ok();
}
}
