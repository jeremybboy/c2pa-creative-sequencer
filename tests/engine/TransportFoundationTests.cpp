#include "transport/TransportFormatting.h"

#include <tracktion_engine/tracktion_engine.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace te = tracktion::engine;

namespace
{
class HostedOnlyEngineBehaviour final : public te::EngineBehaviour
{
public:
    bool autoInitialiseDeviceManager() override
    {
        return false;
    }

    bool addSystemAudioIODeviceTypes() override
    {
        return false;
    }

    bool shouldOpenAudioInputByDefault() override
    {
        return false;
    }
};

bool isSilent(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        if (buffer.getMagnitude(channel, 0, buffer.getNumSamples()) != 0.0f)
            return false;

    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (c2paseq::transport::formatPosition(65.432) != "01:05.432")
        return 1;

    if (c2paseq::transport::sanitisePosition(-4.0) != 0.0)
        return 2;

    te::Engine engine("C2PATransportTests", nullptr,
                      std::make_unique<HostedOnlyEngineBehaviour>());
    auto& deviceManager = engine.getDeviceManager();
    auto& audioInterface = deviceManager.getHostedAudioDeviceInterface();
    te::HostedAudioDeviceInterface::Parameters parameters;
    parameters.sampleRate = c2paseq::transport::preferredSampleRate;
    parameters.blockSize = c2paseq::transport::preferredBlockSize;
    parameters.inputChannels = 0;
    parameters.outputChannels = 2;
    audioInterface.initialise(parameters);
    audioInterface.prepareToPlay(parameters.sampleRate, parameters.blockSize);
    deviceManager.dispatchPendingUpdates();

    auto edit = te::createEmptyEdit(engine, {});
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();

    transport.setPosition(tracktion::TimePosition::fromSeconds(3.25));
    if (std::abs(transport.getPosition().inSeconds() - 3.25) > 0.000001)
        return 3;

    transport.stop(false, false);
    transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));
    transport.play(false);

    juce::AudioBuffer<float> audio(parameters.outputChannels, parameters.blockSize);
    juce::MidiBuffer midi;
    constexpr int blocksToProcess = 64;

    for (int block = 0; block < blocksToProcess; ++block)
    {
        audio.clear();
        audioInterface.processBlock(audio, midi);

        if (! isSilent(audio))
            return 4;
    }

    const auto expectedPosition = static_cast<double>(blocksToProcess * parameters.blockSize)
        / parameters.sampleRate;
    const auto* playbackContext = transport.getCurrentPlaybackContext();
    if (playbackContext == nullptr)
        return 5;

    if (std::abs(playbackContext->getPosition().inSeconds() - expectedPosition) > 0.02)
    {
        std::cerr << "expected position " << expectedPosition
                  << ", got " << playbackContext->getPosition().inSeconds() << '\n';
        return 6;
    }

    transport.stop(false, false);
    transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));

    for (int block = 0; block < 4; ++block)
    {
        audio.clear();
        audioInterface.processBlock(audio, midi);
    }

    if (std::abs(transport.getPosition().inSeconds()) > 0.000001)
    {
        std::cerr << "stop position was " << transport.getPosition().inSeconds() << '\n';
        return 7;
    }

    std::cout << "transport foundation: deterministic silent playback passed\n";
    return 0;
}
