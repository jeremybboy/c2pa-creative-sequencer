#include "TracktionAdapter.h"

#include "app/AppInfo.h"
#include "transport/TransportFormatting.h"

#include <algorithm>
#include <memory>

namespace c2paseq
{
namespace
{
class SequencerEngineBehaviour final : public tracktion::engine::EngineBehaviour
{
public:
    bool shouldOpenAudioInputByDefault() override
    {
        return false;
    }
};
}

TracktionAdapter::TracktionAdapter()
    : engine(appInfo::name.data(), nullptr, std::make_unique<SequencerEngineBehaviour>())
{
    auto& deviceManager = engine.getDeviceManager();

    // An uninitialised hosted interface disables Tracktion's hardware-MIDI path
    // without replacing the selected CoreAudio device. MIDI is out of scope.
    deviceManager.getHostedAudioDeviceInterface();
    deviceManager.setMidiDeviceScanIntervalSeconds(0);
    configurePreferredAudioSettings();

    edit = tracktion::engine::createEmptyEdit(engine, {});
    setBpm(transport::defaultBpm);
    edit->getTransport().setLoopRange({
        tracktion::TimePosition::fromSeconds(0.0),
        tracktion::TimePosition::fromSeconds(transport::defaultLoopEndSeconds)
    });
    edit->getTransport().ensureContextAllocated();
    initialised = true;
}

TracktionAdapter::~TracktionAdapter()
{
    if (edit != nullptr)
        edit->getTransport().stop(false, true);
}

bool TracktionAdapter::isInitialised() const noexcept
{
    return initialised;
}

juce::String TracktionAdapter::audioDeviceDescription() const
{
    const auto snapshot = audioDeviceSnapshot();

    if (! snapshot.open)
        return "Audio unavailable: no output device selected";

    return "Audio: " + snapshot.name
        + " | " + juce::String(snapshot.sampleRate, 0) + " Hz"
        + " | " + juce::String(snapshot.blockSize) + " samples";
}

AudioDeviceSnapshot TracktionAdapter::audioDeviceSnapshot() const
{
    AudioDeviceSnapshot snapshot;

    if (auto* device = engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
    {
        snapshot.name = device->getName();
        snapshot.sampleRate = device->getCurrentSampleRate();
        snapshot.blockSize = device->getCurrentBufferSizeSamples();
        snapshot.open = true;
    }

    return snapshot;
}

TransportSnapshot TracktionAdapter::transportSnapshot() const
{
    if (edit == nullptr)
        return {};

    const auto& editTransport = edit->getTransport();
    const auto* tempo = edit->tempoSequence.getTempo(0);

    return {
        editTransport.getPosition().inSeconds(),
        tempo != nullptr ? tempo->getBpm() : transport::defaultBpm,
        editTransport.isPlaying(),
        editTransport.looping.get()
    };
}

juce::AudioDeviceManager& TracktionAdapter::audioDeviceManager() noexcept
{
    return engine.getDeviceManager().deviceManager;
}

void TracktionAdapter::play()
{
    auto& editTransport = edit->getTransport();
    editTransport.ensureContextAllocated();
    editTransport.play(false);
}

void TracktionAdapter::pause()
{
    edit->getTransport().stop(false, false);
}

void TracktionAdapter::stop()
{
    auto& editTransport = edit->getTransport();
    editTransport.stop(false, false);
    editTransport.setPosition(tracktion::TimePosition::fromSeconds(0.0));
}

void TracktionAdapter::seek(double positionSeconds)
{
    edit->getTransport().setPosition(tracktion::TimePosition::fromSeconds(
        transport::sanitisePosition(positionSeconds)));
}

void TracktionAdapter::setLooping(bool shouldLoop)
{
    edit->getTransport().looping = shouldLoop;
}

void TracktionAdapter::setBpm(double bpm)
{
    if (edit == nullptr)
        return;

    if (auto* tempo = edit->tempoSequence.getTempo(0))
        tempo->setBpm(std::clamp(bpm, transport::minimumBpm, transport::maximumBpm));
}

void TracktionAdapter::configurePreferredAudioSettings()
{
    auto& deviceManager = engine.getDeviceManager().deviceManager;
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (device->getAvailableSampleRates().contains(transport::preferredSampleRate))
        setup.sampleRate = transport::preferredSampleRate;

    if (device->getAvailableBufferSizes().contains(transport::preferredBlockSize))
        setup.bufferSize = transport::preferredBlockSize;

    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();

    const auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isEmpty())
        engine.getDeviceManager().dispatchPendingUpdates();
}
}
