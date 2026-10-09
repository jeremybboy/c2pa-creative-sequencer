#include "InputRecordingService.h"
#include <cmath>

namespace c2paseq
{
InputRecordingService::InputRecordingService() { diskThread.startThread(); }
InputRecordingService::~InputRecordingService()
{
    cancelRecording();
    diskThread.stopThread(5000);
}

void InputRecordingService::setArmed(bool value) noexcept
{
    armed.store(value);
    peak.store(0.0f);
}

void InputRecordingService::prepareInput(double rate, bool available) noexcept
{
    if (isRecording()) failed.store(true);
    inputRate.store(rate);
    inputAvailable.store(available);
}

juce::Result InputRecordingService::startRecording()
{
    if (isRecording() || writer != nullptr || workspace != juce::File())
        return juce::Result::fail("A take is already recording or awaiting cleanup");
    takeRate = inputRate.load();
    if (! armed.load() || ! inputAvailable.load() || ! std::isfinite(takeRate) || takeRate <= 0.0)
        return juce::Result::fail("Arm an available audio input before recording");
    workspace = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("c2paseq-audio-take-" + juce::Uuid().toString());
    if (auto result = workspace.createDirectory(); result.failed())
    { workspace = juce::File(); return result; }
    auto stream = workspace.getChildFile("capture.wav").createOutputStream();
    juce::WavAudioFormat format;
    auto pcm = std::unique_ptr<juce::AudioFormatWriter>(
        stream != nullptr ? format.createWriterFor(stream.release(), takeRate, 1, 24, {}, 0) : nullptr);
    if (! pcm)
    { cancelRecording(); return juce::Result::fail("Could not create the temporary capture WAV"); }
    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(pcm.release(), diskThread, 65536);
    frames.store(0);
    failed.store(false);
    activeWriter.store(writer.get());
    return juce::Result::ok();
}

std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> InputRecordingService::detachWriter()
{
    activeWriter.store(nullptr);
    // The callback never waits for us. Only the non-realtime stop path waits
    // for an already-running callback before flushing/destroying the writer.
    while (callbacks.load() != 0) juce::Thread::yield();
    return std::move(writer);
}

juce::Result InputRecordingService::stopRecording(RecordedAudioTake& take)
{
    if (! isRecording()) return juce::Result::fail("No audio take is recording");
    auto stopped = detachWriter();
    stopped.reset(); // flush queue and finalize WAV header off the audio thread
    if (failed.load() || frames.load() <= 0)
        return juce::Result::fail("Take rejected: input stopped, format changed, capture overflowed, or no samples arrived");
    const auto file = workspace.getChildFile("capture.wav");
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(stream != nullptr
        ? format.createReaderFor(stream.release(), true) : nullptr);
    if (! reader || reader->lengthInSamples != frames.load() || reader->numChannels != 1
        || reader->sampleRate != takeRate || reader->bitsPerSample != 24)
        return juce::Result::fail("The finalized capture WAV did not match the accepted input samples");
    take = { file, takeRate, frames.load() };
    return juce::Result::ok();
}

void InputRecordingService::cancelRecording()
{
    auto stopped = detachWriter();
    stopped.reset();
    if (workspace != juce::File()) workspace.deleteRecursively();
    workspace = juce::File();
}

void InputRecordingService::processInput(const float* input, int count) noexcept
{
    callbacks.fetch_add(1);
    if (armed.load() && input != nullptr && count > 0)
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            if (! std::isfinite(input[i])) failed.store(true);
            blockPeak = std::max(blockPeak, std::abs(input[i]));
        }
        auto previous = peak.load();
        if (blockPeak > previous) peak.store(blockPeak);
        if (auto* target = activeWriter.load())
        {
            const float* mono[] { input };
            if (target->write(mono, count)) frames.fetch_add(count);
            else failed.store(true);
        }
    }
    else if (activeWriter.load() != nullptr) failed.store(true);
    callbacks.fetch_sub(1);
}

void InputRecordingService::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs,
    float* const* outputs, int numOutputs, int count, const juce::AudioIODeviceCallbackContext&)
{
    // JUCE mixes each callback's output. This callback contributes silence only:
    // external input is never copied or monitored through the output.
    for (int i = 0; i < numOutputs; ++i)
        if (outputs[i] != nullptr) juce::FloatVectorOperations::clear(outputs[i], count);
    processInput(numInputs == 1 ? inputs[0] : nullptr, count);
}
void InputRecordingService::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    prepareInput(device != nullptr ? device->getCurrentSampleRate() : 0.0,
                 device != nullptr && device->getActiveInputChannels().countNumberOfSetBits() == 1);
}
void InputRecordingService::audioDeviceStopped() { prepareInput(0.0, false); }
void InputRecordingService::audioDeviceError(const juce::String&) { failed.store(true); }
}
