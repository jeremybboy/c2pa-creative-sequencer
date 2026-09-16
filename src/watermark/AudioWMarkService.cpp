#include "AudioWMarkService.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace c2paseq
{
namespace
{
constexpr double timeoutSeconds = 180.0;
}

AudioWMarkService::AudioWMarkService(juce::File executableFile)
    : executable(std::move(executableFile))
{
}

juce::File AudioWMarkService::defaultExecutable()
{
    const auto override = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_AUDIOWMARK_EXECUTABLE", {});
    if (override.isNotEmpty())
        return juce::File(override);
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("AudioWMark/bin/audiowmark");
}

bool AudioWMarkService::isAvailable() const
{
    return executable.existsAsFile();
}

juce::String AudioWMarkService::statusDescription() const
{
    return isAvailable()
        ? "Ready (external AudioWMark 0.6.5 runtime)"
        : "Setup required: run scripts/setup_audiowmark.sh";
}

juce::Result AudioWMarkService::embed(const juce::File& input,
                                      const juce::File& output,
                                      const SoftBindingPayload& payload,
                                      WatermarkEmbedResult& details,
                                      const std::function<bool()>& shouldCancel)
{
    if (! isAvailable())
        return juce::Result::fail(statusDescription());
    if (! input.existsAsFile())
        return juce::Result::fail("AudioWMark input WAV is missing");

    juce::ChildProcess process;
    const juce::StringArray command { executable.getFullPathName(), "add", "--strict",
                                      input.getFullPathName(), output.getFullPathName(),
                                      payload.toHex() };
    const auto started = juce::Time::getMillisecondCounterHiRes();
    if (! process.start(command))
        return juce::Result::fail("Could not start the external AudioWMark executable");

    while (process.isRunning())
    {
        if (shouldCancel && shouldCancel())
        {
            process.kill();
            output.deleteFile();
            return juce::Result::fail("Export cancelled during AudioWMark embedding");
        }
        if ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0 > timeoutSeconds)
        {
            process.kill();
            output.deleteFile();
            return juce::Result::fail("AudioWMark embedding exceeded the 180-second safety timeout");
        }
        juce::Thread::sleep(20);
    }

    const auto outputText = process.readAllProcessOutput().trim();
    if (process.getExitCode() != 0)
    {
        output.deleteFile();
        return juce::Result::fail("AudioWMark embedding failed: " + outputText);
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(output));
    if (reader == nullptr)
        return juce::Result::fail("AudioWMark produced an unreadable audio file");
    details.elapsedSeconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    details.sampleRate = static_cast<int>(reader->sampleRate);
    details.channels = static_cast<int>(reader->numChannels);
    details.bitsPerSample = static_cast<int>(reader->bitsPerSample);
    details.frames = reader->lengthInSamples;
    return juce::Result::ok();
}
}
