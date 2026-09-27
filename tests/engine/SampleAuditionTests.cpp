#include "engine/SampleAuditionPlayer.h"

#include <cmath>
#include <iostream>

namespace
{
bool writeTone(const juce::File& file, double frequency)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 48000;
    juce::AudioBuffer<float> audio(2, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto value = static_cast<float>(0.1 * std::sin(
            juce::MathConstants<double>::twoPi * frequency * sample / sampleRate));
        audio.setSample(0, sample, value);
        audio.setSample(1, sample, value);
    }
    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 2, 24, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(audio, 0, sampleCount);
}

int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}
}

int main()
{
    const auto root = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-sample-audition", {}, false);
    if (root.createDirectory().failed())
        return fail(1, "could not create sample-audition test directory");

    struct Cleanup
    {
        juce::File directory;
        ~Cleanup() { directory.deleteRecursively(); }
    } cleanup { root };

    const auto first = root.getChildFile("first.wav");
    const auto second = root.getChildFile("second.wav");
    if (! writeTone(first, 220.0) || ! writeTone(second, 330.0))
        return fail(2, "could not create audition fixtures");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::AudioDeviceManager deviceManager;
    c2paseq::SampleAuditionPlayer audition(deviceManager, formats);

    if (audition.preview(root.getChildFile("missing.wav")).wasOk())
        return fail(3, "missing sample was accepted");
    if (audition.preview(first).failed() || audition.currentFile() != first
        || ! audition.isPlaying())
        return fail(4, "first sample did not start");
    if (audition.preview(second).failed() || audition.currentFile() != second
        || ! audition.isPlaying())
        return fail(5, "second sample did not replace the first");
    if (audition.preview(root.getChildFile("missing.wav")).wasOk()
        || audition.isPlaying() || audition.currentFile() != juce::File())
        return fail(6, "failed replacement did not stop the previous audition");

    if (audition.preview(first).failed())
        return fail(7, "sample could not restart after a failed replacement");

    audition.stop();
    if (audition.isPlaying() || audition.currentFile() != juce::File())
        return fail(8, "stop did not clear the active audition");

    std::cout << "explicit single-sample audition and stop passed\n";
    return 0;
}
