#include "WavMarkService.h"

namespace c2paseq
{
WavMarkService::WavMarkService(juce::File runtimeDirectory)
    : runtime(std::move(runtimeDirectory))
{
}

juce::File WavMarkService::defaultRuntimeDirectory()
{
    const auto override = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_WAVMARK_RUNTIME", {});
    if (override.isNotEmpty())
        return juce::File(override);
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("WavMark");
}

juce::File WavMarkService::pythonExecutable() const
{
    return runtime.getChildFile("venv/bin/python3");
}

juce::File WavMarkService::helperScript() const
{
    return runtime.getChildFile("wavmark_helper.py");
}

bool WavMarkService::isAvailable() const
{
    return pythonExecutable().existsAsFile() && helperScript().existsAsFile()
        && runtime.getChildFile("model.model.pkl").existsAsFile();
}

juce::String WavMarkService::statusDescription() const
{
    return isAvailable()
        ? "Ready (offline machine-local WavMark runtime)"
        : "Setup required: run scripts/setup_wavmark.sh";
}

juce::Result WavMarkService::run(const juce::StringArray& arguments, juce::var& response) const
{
    if (! isAvailable())
        return juce::Result::fail(statusDescription());
    juce::StringArray command { pythonExecutable().getFullPathName(),
                                helperScript().getFullPathName() };
    command.addArray(arguments);
    juce::ChildProcess process;
    if (! process.start(command))
        return juce::Result::fail("Could not start the machine-local WavMark helper");
    if (! process.waitForProcessToFinish(10 * 60 * 1000))
    {
        process.kill();
        return juce::Result::fail("WavMark helper timed out");
    }
    const auto output = process.readAllProcessOutput().trim();
    if (process.getExitCode() != 0)
        return juce::Result::fail("WavMark helper failed: " + output);
    if (const auto parsed = juce::JSON::parse(output, response); parsed.failed()
        || response.getDynamicObject() == nullptr)
        return juce::Result::fail("WavMark helper returned invalid JSON: " + output);
    if (! static_cast<bool>(response.getProperty("ok", false)))
        return juce::Result::fail(response.getProperty("error", "WavMark operation failed").toString());
    return juce::Result::ok();
}

juce::Result WavMarkService::embed(const juce::File& input,
                                   const juce::File& output,
                                   const SoftBindingPayload& payload,
                                   WatermarkEmbedResult& details)
{
    juce::var response;
    const auto result = run({ "embed", "--model",
        runtime.getChildFile("model.model.pkl").getFullPathName(), "--input",
        input.getFullPathName(), "--output", output.getFullPathName(),
        "--payload", payload.toHex() }, response);
    if (result.failed()) return result;
    details.snrDb = static_cast<double>(response.getProperty("snr_db", 0.0));
    details.sampleRate = static_cast<int>(response.getProperty("sample_rate", 0));
    details.channels = static_cast<int>(response.getProperty("channels", 0));
    details.frames = static_cast<juce::int64>(response.getProperty("frames", 0));
    if (response.getProperty("payload_hex", {}).toString() != payload.toHex())
        return juce::Result::fail("WavMark helper did not verify the requested payload");
    return juce::Result::ok();
}

juce::Result WavMarkService::decode(const juce::File& input, SoftBindingPayload& payload)
{
    juce::var response;
    const auto result = run({ "decode", "--model",
        runtime.getChildFile("model.model.pkl").getFullPathName(), "--input",
        input.getFullPathName() }, response);
    if (result.failed()) return result;
    const auto decoded = SoftBindingPayload::fromHex(
        response.getProperty("payload_hex", {}).toString());
    if (! decoded.has_value())
        return juce::Result::fail("WavMark decoded no valid 16-bit payload");
    payload = *decoded;
    return juce::Result::ok();
}
}
