#include "SampleAuditionPlayer.h"

namespace c2paseq
{
SampleAuditionPlayer::SampleAuditionPlayer(juce::AudioDeviceManager& manager,
                                           juce::AudioFormatManager& formats)
    : deviceManager(manager), formatManager(formats)
{
    output.setSource(&transport);
    deviceManager.addAudioCallback(&output);
}

SampleAuditionPlayer::~SampleAuditionPlayer()
{
    stop();
    deviceManager.removeAudioCallback(&output);
    output.setSource(nullptr);
}

juce::Result SampleAuditionPlayer::preview(const juce::File& source)
{
    stop();

    if (! source.existsAsFile())
        return juce::Result::fail("Sample is missing");

    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(source));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return juce::Result::fail("Sample format could not be decoded");

    const auto sourceSampleRate = reader->sampleRate;
    auto replacement = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    replacement->setLooping(false);

    readerSource = std::move(replacement);
    file = source;
    transport.setSource(readerSource.get(), 0, nullptr, sourceSampleRate);
    transport.setPosition(0.0);
    transport.start();
    return juce::Result::ok();
}

void SampleAuditionPlayer::stop()
{
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();
    file = juce::File();
}

bool SampleAuditionPlayer::isPlaying() const noexcept
{
    return transport.isPlaying();
}

const juce::File& SampleAuditionPlayer::currentFile() const noexcept
{
    return file;
}
}
