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
    bool autoInitialiseDeviceManager() override
    {
        return false;
    }

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

    // Configure MIDI before the first device scan. The hosted interface prevents
    // hardware enumeration; disabling its placeholders prevents MIDI ports from
    // opening. It remains uninitialised, so CoreAudio is still selected.
    juce::XmlElement disabledMidi("SETTINGS");
    disabledMidi.setAttribute("enabled", false);
    auto& settings = engine.getPropertyStorage();
    settings.setXmlPropertyItem(tracktion::engine::SettingID::midiout,
                                "MIDI Output", disabledMidi);
    settings.setXmlPropertyItem(tracktion::engine::SettingID::midiin,
                                "MIDI Input", disabledMidi);
    settings.setXmlPropertyItem(tracktion::engine::SettingID::virtualmidiin,
                                "All MIDI Ins", disabledMidi);
    deviceManager.getHostedAudioDeviceInterface();
    deviceManager.setMidiDeviceScanIntervalSeconds(0);
    deviceManager.initialise(0, tracktion::engine::DeviceManager::defaultNumChannelsToOpen);
    configurePreferredAudioSettings();

    edit = tracktion::engine::createEmptyEdit(engine, {});
    prepareEdit();
    initialised = true;
}

void TracktionAdapter::prepareEdit()
{
    setBpm(transport::defaultBpm);
    edit->getTransport().setLoopRange({
        tracktion::TimePosition::fromSeconds(0.0),
        tracktion::TimePosition::fromSeconds(transport::defaultLoopEndSeconds)
    });
    edit->getTransport().ensureContextAllocated();
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

juce::Result TracktionAdapter::inspectAudioFile(const juce::File& file,
                                                 AudioFileMetadata& metadata)
{
    if (! file.existsAsFile())
        return juce::Result::fail("Audio file does not exist");
    if (! file.hasFileExtension("wav;aif;aiff;mp3"))
        return juce::Result::fail("Supported audio formats are WAV, AIFF, and MP3");

    tracktion::engine::AudioFile audioFile(engine, file);
    if (! audioFile.isValid())
        return juce::Result::fail("The file could not be decoded as audio");

    metadata.lengthSeconds = audioFile.getLength();
    metadata.sampleRate = audioFile.getSampleRate();
    metadata.channels = audioFile.getNumChannels();
    if (metadata.lengthSeconds <= 0.0 || metadata.sampleRate <= 0.0 || metadata.channels <= 0)
        return juce::Result::fail("The audio file has invalid stream metadata");
    return juce::Result::ok();
}

juce::Result TracktionAdapter::insertAudioClip(const juce::File& file,
                                                const juce::String& name,
                                                int trackIndex,
                                                double startSeconds,
                                                double lengthSeconds)
{
    if (edit == nullptr || trackIndex < 0 || startSeconds < 0.0 || lengthSeconds <= 0.0)
        return juce::Result::fail("Invalid audio clip placement");

    edit->ensureNumberOfAudioTracks(trackIndex + 1);
    const auto audioTracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, audioTracks.size()))
        return juce::Result::fail("Could not create an audio track");

    auto* track = audioTracks[trackIndex];
    track->setName(name);
    const auto start = tracktion::TimePosition::fromSeconds(startSeconds);
    const tracktion::engine::ClipPosition clipPosition {
        { start, start + tracktion::TimeDuration::fromSeconds(lengthSeconds) },
        {}
    };
    if (track->insertWaveClip(name, file, clipPosition, false) == nullptr)
        return juce::Result::fail("Tracktion could not create the audio clip");

    edit->getTransport().ensureContextAllocated(true);
    return juce::Result::ok();
}

juce::AudioFormatManager& TracktionAdapter::audioFormatManager() noexcept
{
    return engine.getAudioFileFormatManager().readFormatManager;
}

juce::AudioThumbnailCache& TracktionAdapter::audioThumbnailCache() noexcept
{
    return engine.getAudioFileManager().getAudioThumbnailCache();
}

bool TracktionAdapter::createProjectEdit(const juce::File& editFile)
{
    auto replacement = tracktion::engine::createEmptyEdit(engine, editFile);
    if (replacement == nullptr)
        return false;

    if (edit != nullptr)
        edit->getTransport().stop(false, true);
    edit = std::move(replacement);
    prepareEdit();
    return true;
}

bool TracktionAdapter::saveProjectEdit(const juce::File& editFile)
{
    if (edit == nullptr)
        return false;

    tracktion::engine::EditFileOperations fileOperations(*edit);
    return fileOperations.writeToFile(editFile, false);
}

bool TracktionAdapter::loadProjectEdit(const juce::File& editFile)
{
    if (! editFile.existsAsFile())
        return false;

    auto replacement = tracktion::engine::loadEditFromFile(engine, editFile);
    if (replacement == nullptr)
        return false;

    if (edit != nullptr)
        edit->getTransport().stop(false, true);
    edit = std::move(replacement);
    edit->getTransport().ensureContextAllocated();
    return true;
}

void TracktionAdapter::closeProjectEdit()
{
    if (edit != nullptr)
        edit->getTransport().stop(false, true);
    edit = tracktion::engine::createEmptyEdit(engine, {});
    prepareEdit();
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
