#include "TracktionAdapter.h"

#include "app/AppInfo.h"
#include "engine/NativeAudioClipPolicy.h"
#include "engine/TrackLevelMeterPlugin.h"
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
    engine.getPluginManager().createBuiltInType<TrackLevelMeterPlugin>();
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
    const auto loopRange = editTransport.getLoopRange();

    return {
        editTransport.getPosition().inSeconds(),
        tempo != nullptr ? tempo->getBpm() : transport::defaultBpm,
        editTransport.isPlaying(),
        editTransport.looping.get(),
        loopRange.getStart().inSeconds(),
        loopRange.getEnd().inSeconds()
    };
}

TrackLevelSnapshot TracktionAdapter::trackLevelSnapshot(int trackIndex) noexcept
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return {};

    if (auto* meter = tracks[trackIndex]->pluginList
            .findFirstPluginOfType<TrackLevelMeterPlugin>())
        return meter->consumePeaks();
    return {};
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

void TracktionAdapter::setLoopRange(double startSeconds, double endSeconds)
{
    startSeconds = transport::sanitisePosition(startSeconds);
    endSeconds = std::max(endSeconds, startSeconds + 0.001);
    edit->getTransport().setLoopRange({
        tracktion::TimePosition::fromSeconds(startSeconds),
        tracktion::TimePosition::fromSeconds(endSeconds)
    });
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
                                                double sourceOffsetSeconds,
                                                double lengthSeconds)
{
    if (edit == nullptr || trackIndex < 0 || startSeconds < 0.0
        || sourceOffsetSeconds < 0.0 || lengthSeconds <= 0.0)
        return juce::Result::fail("Invalid audio clip placement");

    edit->ensureNumberOfAudioTracks(trackIndex + 1);
    const auto audioTracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, audioTracks.size()))
        return juce::Result::fail("Could not create an audio track");

    auto* track = audioTracks[trackIndex];
    const auto start = tracktion::TimePosition::fromSeconds(startSeconds);
    const tracktion::engine::ClipPosition clipPosition {
        { start, start + tracktion::TimeDuration::fromSeconds(lengthSeconds) },
        tracktion::TimeDuration::fromSeconds(sourceOffsetSeconds)
    };
    const auto newClip = track->insertWaveClip(name, file, clipPosition, false);
    if (newClip == nullptr)
        return juce::Result::fail("Tracktion could not create the audio clip");

    configureNativeAudioClip(*newClip, lengthSeconds);

    edit->getTransport().ensureContextAllocated(true);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::insertMidiClip(
    const juce::String& name,
    int trackIndex,
    double startBeats,
    double lengthBeats,
    const std::vector<MidiPlaybackNote>& notes)
{
    if (edit == nullptr || trackIndex < 0 || startBeats < 0.0 || lengthBeats <= 0.0)
        return juce::Result::fail("Invalid MIDI clip placement");

    edit->ensureNumberOfAudioTracks(trackIndex + 1);
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Could not create a MIDI playback track");

    const auto start = tracktion::BeatPosition::fromBeats(startBeats);
    const auto end = tracktion::BeatPosition::fromBeats(startBeats + lengthBeats);
    auto clip = tracks[trackIndex]->insertMIDIClip(
        name, edit->tempoSequence.toTime({ start, end }), nullptr);
    if (clip == nullptr)
        return juce::Result::fail("Tracktion could not create the MIDI clip");

    auto& sequence = clip->getSequence();
    for (const auto& note : notes)
    {
        if (! juce::isPositiveAndBelow(note.noteNumber, 128)
            || note.startBeats < 0.0 || note.durationBeats <= 0.0
            || note.startBeats >= lengthBeats)
            return juce::Result::fail("MIDI clip contains an invalid note");
        const auto duration = std::min(note.durationBeats,
                                       lengthBeats - note.startBeats);
        sequence.addNote(note.noteNumber,
                         tracktion::BeatPosition::fromBeats(note.startBeats),
                         tracktion::BeatDuration::fromBeats(duration),
                         juce::jlimit(1, 127, note.velocity), 0, nullptr);
    }

    edit->getTransport().ensureContextAllocated(true);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::setTrackProperties(int trackIndex,
                                                   const juce::String& name,
                                                   double gainDb,
                                                   double pan,
                                                   bool muted,
                                                   bool soloed)
{
    if (edit == nullptr || trackIndex < 0)
        return juce::Result::fail("Invalid audio track");

    edit->ensureNumberOfAudioTracks(trackIndex + 1);
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Could not create audio track");

    auto* track = tracks[trackIndex];
    ensureTrackLevelMeter(*track);
    track->setName(name);
    track->setMute(muted);
    track->setSolo(soloed);
    if (auto* volume = track->getVolumePlugin())
    {
        volume->setVolumeDb(static_cast<float>(juce::jlimit(-60.0, 12.0, gainDb)));
        volume->setPan(static_cast<float>(juce::jlimit(-1.0, 1.0, pan)));
    }
    return juce::Result::ok();
}

void TracktionAdapter::ensureTrackLevelMeter(tracktion::engine::AudioTrack& track)
{
    if (track.pluginList.findFirstPluginOfType<TrackLevelMeterPlugin>() != nullptr)
        return;

    track.pluginList.insertPlugin(TrackLevelMeterPlugin::create(), -1);
}

juce::Result TracktionAdapter::setTrackMute(int trackIndex, bool muted)
{
    if (edit == nullptr || trackIndex < 0)
        return juce::Result::fail("Invalid audio track");
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    tracks[trackIndex]->setMute(muted);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::setTrackSolo(int trackIndex, bool soloed)
{
    if (edit == nullptr || trackIndex < 0)
        return juce::Result::fail("Invalid audio track");
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    tracks[trackIndex]->setSolo(soloed);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::setTrackGain(int trackIndex, double gainDb)
{
    if (edit == nullptr || trackIndex < 0)
        return juce::Result::fail("Invalid audio track");
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    if (auto* volume = tracks[trackIndex]->getVolumePlugin())
    {
        volume->setVolumeDb(static_cast<float>(juce::jlimit(-60.0, 12.0, gainDb)));
        return juce::Result::ok();
    }
    return juce::Result::fail("Audio track has no volume control");
}

juce::Result TracktionAdapter::setTrackPan(int trackIndex, double pan)
{
    if (edit == nullptr || trackIndex < 0)
        return juce::Result::fail("Invalid audio track");
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    if (auto* volume = tracks[trackIndex]->getVolumePlugin())
    {
        volume->setPan(static_cast<float>(juce::jlimit(-1.0, 1.0, pan)));
        return juce::Result::ok();
    }
    return juce::Result::fail("Audio track has no pan control");
}

juce::Result TracktionAdapter::sendLiveMidiMessage(
    int trackIndex, const juce::MidiMessage& message)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("MIDI monitoring track was not found");
    auto* instance = trackPluginInstance(trackIndex);
    if (instance == nullptr || ! instance->acceptsMidi())
        return juce::Result::fail("Selected MIDI track has no active instrument");

    edit->getTransport().ensureContextAllocated();
    tracks[trackIndex]->injectLiveMidiMessage(message, {});
    return juce::Result::ok();
}

void TracktionAdapter::allNotesOff(int trackIndex)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return;
    tracks[trackIndex]->injectLiveMidiMessage(juce::MidiMessage::allNotesOff(1), {});
}

void TracktionAdapter::registerPluginDescription(const juce::PluginDescription& description)
{
    engine.getPluginManager().knownPluginList.addType(description);
}

juce::Result TracktionAdapter::setTrackPlugin(int trackIndex,
                                               const juce::PluginDescription& description,
                                               const juce::String& stateBase64,
                                               bool bypassed)
{
    if (edit == nullptr || trackIndex < 0 || description.pluginFormatName != "VST3")
        return juce::Result::fail("Invalid VST3 plug-in");

    edit->ensureNumberOfAudioTracks(trackIndex + 1);
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");

    registerPluginDescription(description);
    if (auto result = removeTrackPlugin(trackIndex); result.failed())
        return result;

    auto state = tracktion::engine::ExternalPlugin::create(engine, description);
    state.setProperty("enabled", ! bypassed, nullptr);
    if (stateBase64.isNotEmpty())
        state.setProperty("state", stateBase64, nullptr);

    auto plugin = edit->getPluginCache().createNewPlugin(state);
    auto* external = dynamic_cast<tracktion::engine::ExternalPlugin*>(plugin.get());
    if (external == nullptr)
        return juce::Result::fail("Tracktion could not create the VST3 node");

    tracks[trackIndex]->pluginList.insertPlugin(plugin, 0, nullptr);
    external->initialiseFully();
    if (const auto error = external->getLoadError(); error.isNotEmpty())
    {
        external->deleteFromParent();
        return juce::Result::fail("VST3 load failed: " + error);
    }

    if (stateBase64.isNotEmpty())
    {
        juce::MemoryBlock restoredState;
        if (! restoredState.fromBase64Encoding(stateBase64))
        {
            external->deleteFromParent();
            return juce::Result::fail("Saved VST3 state is invalid");
        }
        if (auto* instance = external->getAudioPluginInstance())
            instance->setStateInformation(restoredState.getData(),
                                          static_cast<int>(restoredState.getSize()));
    }

    external->setEnabled(! bypassed);
    edit->getTransport().ensureContextAllocated(true);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::setTrackPluginBypassed(int trackIndex, bool bypassed)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    auto* plugin = tracks[trackIndex]->pluginList
        .findFirstPluginOfType<tracktion::engine::ExternalPlugin>();
    if (plugin == nullptr)
        return juce::Result::fail("Track has no loaded VST3");
    plugin->setEnabled(! bypassed);
    return juce::Result::ok();
}

juce::Result TracktionAdapter::removeTrackPlugin(int trackIndex)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    if (auto* plugin = tracks[trackIndex]->pluginList
            .findFirstPluginOfType<tracktion::engine::ExternalPlugin>())
        plugin->deleteFromParent();
    return juce::Result::ok();
}

juce::Result TracktionAdapter::captureTrackPluginState(int trackIndex,
                                                        juce::String& stateBase64,
                                                        bool& bypassed,
                                                        bool& missing)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("Audio track was not found");
    auto* plugin = tracks[trackIndex]->pluginList
        .findFirstPluginOfType<tracktion::engine::ExternalPlugin>();
    if (plugin == nullptr)
        return juce::Result::fail("Track has no loaded VST3");

    plugin->flushPluginStateToValueTree();
    if (auto* instance = plugin->getAudioPluginInstance())
    {
        juce::MemoryBlock state;
        instance->getStateInformation(state);
        stateBase64 = state.toBase64Encoding();
    }
    else
    {
        stateBase64 = plugin->state.getProperty("state").toString();
    }
    bypassed = ! plugin->isEnabled();
    missing = plugin->isMissing();
    return juce::Result::ok();
}

juce::AudioPluginInstance* TracktionAdapter::trackPluginInstance(int trackIndex) const
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return nullptr;
    if (auto* plugin = tracks[trackIndex]->pluginList
            .findFirstPluginOfType<tracktion::engine::ExternalPlugin>())
        return plugin->getAudioPluginInstance();
    return nullptr;
}

juce::Result TracktionAdapter::renderWav(const juce::File& destination,
                                         double endSeconds)
{
    return renderWavInternal(destination, endSeconds, -1);
}

juce::Result TracktionAdapter::renderTrackWav(const juce::File& destination,
                                              int trackIndex, double endSeconds,
                                              double startSeconds, bool includeTrackMix)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("The stem source track was not found");
    if (! destination.hasFileExtension("wav") || destination.isDirectory()
        || ! std::isfinite(startSeconds) || ! std::isfinite(endSeconds)
        || startSeconds < 0.0 || endSeconds <= startSeconds)
        return juce::Result::fail("A stem requires a WAV destination and valid render range");
    // Synchronous callers own their scope; the UI establishes it before its worker starts.
    const auto ownsScope = ! exclusiveTrackRender;
    if (ownsScope)
        if (auto result = beginExclusiveTrackRender(trackIndex, includeTrackMix);
            result.failed()) return result;
    const auto result = renderWavInternal(destination, endSeconds, trackIndex, startSeconds);
    if (ownsScope) finishExclusiveTrackRender();
    return result;
}

juce::Result TracktionAdapter::beginExclusiveTrackRender(int trackIndex, bool includeTrackMix)
{
    if (edit == nullptr || exclusiveTrackRender)
        return juce::Result::fail("An offline track render is already active or no edit is open");
    renderOriginalPosition = transportSnapshot().positionSeconds;
    tracktion::engine::TransportControl::stopAllTransports(engine, false, true);
    renderAudibility.clear();
    for (auto* track : tracktion::engine::getAudioTracks(*edit))
    {
        renderAudibility.emplace_back(track->isMuted(false), track->isSolo(false));
        track->setMute(false);
        track->setSolo(false);
    }
    edit->updateMuteSoloStatuses();
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    if (! includeTrackMix && juce::isPositiveAndBelow(trackIndex, tracks.size()))
    {
        renderVolumePlugin = tracks[trackIndex]->getVolumePlugin();
        if (renderVolumePlugin != nullptr)
        {
            renderVolumeWasEnabled = renderVolumePlugin->isEnabled();
            renderVolumePlugin->setEnabled(false);
        }
    }
    exclusiveTrackRender = true;
    return juce::Result::ok();
}

void TracktionAdapter::finishExclusiveTrackRender()
{
    if (! exclusiveTrackRender || edit == nullptr) return;
    const auto tracks = tracktion::engine::getAudioTracks(*edit);
    for (int index = 0; index < std::min(tracks.size(), static_cast<int>(renderAudibility.size())); ++index)
    {
        const auto& saved = renderAudibility[static_cast<std::size_t>(index)];
        tracks[index]->setMute(saved.first);
        tracks[index]->setSolo(saved.second);
    }
    edit->updateMuteSoloStatuses();
    if (renderVolumePlugin != nullptr)
        renderVolumePlugin->setEnabled(renderVolumeWasEnabled);
    renderVolumePlugin = nullptr;
    seek(renderOriginalPosition);
    renderAudibility.clear();
    exclusiveTrackRender = false;
}

juce::Result TracktionAdapter::renderWavInternal(const juce::File& destination,
                                                 double endSeconds, int onlyTrackIndex,
                                                 double startSeconds)
{
    if (edit == nullptr || ! std::isfinite(startSeconds) || ! std::isfinite(endSeconds)
        || startSeconds < 0.0 || endSeconds <= startSeconds)
        return juce::Result::fail("Invalid render range");

    tracktion::engine::Renderer::Parameters parameters(*edit);
    parameters.destFile = destination;
    parameters.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    parameters.bitDepth = 24;
    parameters.sampleRateForAudio = engine.getDeviceManager().getSampleRate();
    if (parameters.sampleRateForAudio <= 0.0)
        parameters.sampleRateForAudio = transport::preferredSampleRate;
    parameters.blockSizeForAudio = transport::preferredBlockSize;
    parameters.time = { tracktion::TimePosition::fromSeconds(startSeconds),
                        tracktion::TimePosition::fromSeconds(endSeconds) };
    parameters.tracksToDo = tracktion::engine::toBitSet(
        tracktion::engine::getAllTracks(*edit));
    if (onlyTrackIndex >= 0)
    {
        const auto audioTracks = tracktion::engine::getAudioTracks(*edit);
        const auto allTracks = tracktion::engine::getAllTracks(*edit);
        parameters.tracksToDo.clear();
        parameters.tracksToDo.setBit(allTracks.indexOf(audioTracks[onlyTrackIndex]));
    }
    parameters.canRenderInMono = false;
    parameters.mustRenderInMono = false;
    parameters.usePlugins = true;
    parameters.useMasterPlugins = onlyTrackIndex < 0;
    parameters.trimSilenceAtEnds = false;
    parameters.shouldNormalise = false;

    tracktion::engine::TransportControl::stopAllTransports(engine, false, true);
    edit->getTransport().freePlaybackContext();
    tracktion::engine::Renderer::turnOffAllPlugins(*edit);
    auto task = tracktion::engine::render_utils::createRenderTask(
        parameters, "Export Mix", nullptr, nullptr);
    if (task == nullptr)
    {
        if (! offlineExportActive) edit->getTransport().ensureContextAllocated(true);
        return juce::Result::fail("Tracktion could not create the offline render task");
    }
    while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
    {
    }
    tracktion::engine::Renderer::turnOffAllPlugins(*edit);
    if (! offlineExportActive) edit->getTransport().ensureContextAllocated(true);
    if (task->errorMessage.isNotEmpty())
    {
        destination.deleteFile();
        return juce::Result::fail("Tracktion offline WAV render failed: "
                                  + task->errorMessage);
    }
    if (! destination.existsAsFile())
        return juce::Result::fail("Tracktion offline WAV render produced no file");
    return juce::Result::ok();
}

juce::Result TracktionAdapter::beginOfflineExport()
{
    if (edit == nullptr || offlineExportActive || exclusiveTrackRender)
        return juce::Result::fail("An offline export is active or no edit is open");
    offlineExportPosition = transportSnapshot().positionSeconds;
    offlineExportInhibitor = std::make_unique<tracktion::engine::TransportControl::ReallocationInhibitor>(
        edit->getTransport());
    tracktion::engine::TransportControl::stopAllTransports(engine, false, true);
    edit->getTransport().freePlaybackContext();
    offlineExportActive = true;
    return juce::Result::ok();
}

juce::Result TracktionAdapter::substituteMidiExportStem(int trackIndex, const juce::File& file,
                                                        double startSeconds, double lengthSeconds)
{
    const auto tracks = edit != nullptr ? tracktion::engine::getAudioTracks(*edit)
                                        : juce::Array<tracktion::engine::AudioTrack*> {};
    if (! offlineExportActive || ! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::Result::fail("No active MIDI export source track");
    auto* track = tracks[trackIndex];
    auto* instrument = track->pluginList.findFirstPluginOfType<tracktion::engine::ExternalPlugin>();
    if (instrument == nullptr || ! instrument->isEnabled() || instrument->isMissing())
        return juce::Result::fail("The export instrument is not available");
    AudioFileMetadata metadata;
    if (auto result = inspectAudioFile(file, metadata); result.failed()) return result;
    if (! std::isfinite(startSeconds) || ! std::isfinite(lengthSeconds) || startSeconds < 0.0
        || lengthSeconds <= 0.0 || std::abs(lengthSeconds - metadata.lengthSeconds) > 0.05)
        return juce::Result::fail("The MIDI export stem has an invalid range");
    MidiExportReplacement replacement;
    replacement.instrument = instrument;
    replacement.instrumentEnabled = instrument->isEnabled();
    for (auto* clip : track->getClips())
        if (clip->isMidi()) replacement.midi.emplace_back(clip, clip->isMuted());
    const auto start = tracktion::TimePosition::fromSeconds(startSeconds);
    replacement.audio = track->insertWaveClip("Credentialed MIDI export stem", file,
        { { start, start + tracktion::TimeDuration::fromSeconds(metadata.lengthSeconds) }, {} }, false);
    if (replacement.audio == nullptr)
        return juce::Result::fail("Could not stage the signed MIDI stem in the export graph");
    configureNativeAudioClip(*static_cast<tracktion::engine::WaveAudioClip*>(replacement.audio.get()),
                             metadata.lengthSeconds);
    for (const auto& original : replacement.midi) original.first->setMuted(true);
    // The signed pre-mixer audio replaces instrument execution, not its track mixer.
    instrument->setEnabled(false);
    midiExportReplacements.push_back(std::move(replacement));
    return juce::Result::ok();
}

void TracktionAdapter::finishOfflineExport()
{
    if (! offlineExportActive || edit == nullptr) return;
    edit->getTransport().stop(false, true);
    edit->getTransport().freePlaybackContext();
    for (auto& replacement : midiExportReplacements)
    {
        replacement.audio->removeFromParent();
        for (const auto& original : replacement.midi) original.first->setMuted(original.second);
        replacement.instrument->setEnabled(replacement.instrumentEnabled);
    }
    midiExportReplacements.clear();
    seek(offlineExportPosition);
    offlineExportActive = false;
    offlineExportInhibitor.reset();
    edit->getTransport().ensureContextAllocated(true);
}

juce::AudioFormatManager& TracktionAdapter::audioFormatManager() noexcept
{
    return engine.getAudioFileFormatManager().readFormatManager;
}

juce::AudioThumbnailCache& TracktionAdapter::audioThumbnailCache() noexcept
{
    return engine.getAudioFileManager().getAudioThumbnailCache();
}

void TracktionAdapter::setBeforeEditReplacement(std::function<void()> callback)
{
    beforeEditReplacement = std::move(callback);
}

bool TracktionAdapter::createProjectEdit(const juce::File& editFile)
{
    auto replacement = tracktion::engine::createEmptyEdit(engine, editFile);
    if (replacement == nullptr)
        return false;

    if (edit != nullptr)
    {
        if (beforeEditReplacement)
            beforeEditReplacement();
        edit->getTransport().stop(false, true);
    }
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
    {
        if (beforeEditReplacement)
            beforeEditReplacement();
        edit->getTransport().stop(false, true);
    }
    edit = std::move(replacement);
    configureLoadedAudioClips();
    edit->getTransport().ensureContextAllocated();
    return true;
}

void TracktionAdapter::closeProjectEdit()
{
    if (edit != nullptr)
    {
        if (beforeEditReplacement)
            beforeEditReplacement();
        edit->getTransport().stop(false, true);
    }
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

void TracktionAdapter::configureLoadedAudioClips()
{
    if (edit == nullptr)
        return;

    for (auto* track : tracktion::engine::getAudioTracks(*edit))
    {
        ensureTrackLevelMeter(*track);
        for (auto* clip : track->getClips())
        {
            auto* waveClip = dynamic_cast<tracktion::engine::WaveAudioClip*>(clip);
            if (waveClip == nullptr)
                continue;

            const auto hasAutomaticStretchState = waveClip->getAutoTempo()
                || waveClip->getAutoPitch()
                || waveClip->getTimeStretchMode()
                    != tracktion::engine::TimeStretcher::disabled
                || std::abs(waveClip->getSpeedRatio() - 1.0) > 0.000001;
            if (! hasAutomaticStretchState)
                continue;

            const tracktion::engine::AudioFile source(engine, waveClip->getOriginalFile());
            if (source.isValid())
                configureNativeAudioClip(*waveClip, source.getLength());
        }
    }
}
}
