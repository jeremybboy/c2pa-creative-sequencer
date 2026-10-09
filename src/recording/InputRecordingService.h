#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>

namespace c2paseq
{
struct RecordedAudioTake
{
    juce::File file;
    double sampleRate = 0.0;
    juce::int64 frames = 0;
    double durationSeconds() const { return sampleRate > 0.0 ? static_cast<double>(frames) / sampleRate : 0.0; }
};

// One producer (the device callback), owner/worker-controlled writer lifecycle.
// No file IO, allocation, locks or signing in the audio callback.
class InputRecordingService final : public juce::AudioIODeviceCallback
{
public:
    InputRecordingService();
    ~InputRecordingService() override;
    void setArmed(bool value) noexcept;
    void prepareInput(double rate, bool available) noexcept;
    juce::Result startRecording();
    juce::Result stopRecording(RecordedAudioTake&);
    void cancelRecording();
    bool isRecording() const noexcept { return activeWriter.load() != nullptr; }
    bool captureFailed() const noexcept { return failed.load(); }
    float consumePeak() noexcept { return peak.exchange(0.0f); }
    // Also used by deterministic synthetic-input tests; production calls this
    // exclusively from audioDeviceIOCallbackWithContext.
    void processInput(const float*, int samples) noexcept;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int,
                                          int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String&) override;

private:
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> detachWriter();
    juce::TimeSliceThread diskThread { "Audio capture WAV writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<int> callbacks { 0 };
    std::atomic_bool armed { false }, inputAvailable { false }, failed { false };
    std::atomic<double> inputRate { 0.0 };
    std::atomic<juce::int64> frames { 0 };
    std::atomic<float> peak { 0.0f };
    juce::File workspace;
    double takeRate = 0.0;
};
}
