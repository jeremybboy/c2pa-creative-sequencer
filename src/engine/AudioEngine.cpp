#include "AudioEngine.h"

#include "export/ExportController.h"
#include "app/AppInfo.h"

namespace c2paseq
{
juce::Result AudioEngine::prepareMidiStem(int trackIndex, MidiStemPlan& plan)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Finish recording before bouncing MIDI");
    const auto result = projectEngine.prepareMidiStem(trackIndex, plan);
    if (result.wasOk()) pluginHost.closeEditorsForOfflineRender();
    return result;
}

juce::Result AudioEngine::beginMidiStemRender(const MidiStemPlan& plan)
{
    return tracktion.beginExclusiveTrackRender(plan.trackIndex, false);
}

void AudioEngine::finishMidiStemRender()
{
    tracktion.finishExclusiveTrackRender();
}

juce::Result AudioEngine::renderAndSignMidiStem(const MidiStemPlan& plan,
                                                const juce::File& unsignedWav,
                                                const juce::File& signedWav)
{
    return projectEngine.renderAndSignMidiStem(plan, unsignedWav, signedWav);
}

juce::Result AudioEngine::importMidiStem(const MidiStemPlan& plan,
                                        const juce::File& signedWav,
                                        juce::String& createdClipId)
{
    return projectEngine.importMidiStem(plan, signedWav, createdClipId);
}

AudioEngine::AudioEngine(std::unique_ptr<SigningProvider> signingProvider,
                         juce::File pluginCacheFile,
                         bool showPluginWindows,
                         std::unique_ptr<WatermarkService> watermarkService,
                         juce::File softBindingOutboxDirectory,
                         std::unique_ptr<FingerprintService> fingerprintService,
                         bool useHardwareRecordingInput)
    : provenance(std::move(signingProvider)),
      watermark(watermarkService != nullptr
          ? std::move(watermarkService) : std::make_unique<AudioWMarkService>()),
      fingerprint(fingerprintService != nullptr
          ? std::move(fingerprintService) : std::make_unique<AudfprintService>()),
      softBindingOutbox(softBindingOutboxDirectory == juce::File()
          ? SoftBindingOutbox::defaultDirectory() : std::move(softBindingOutboxDirectory)),
      projectEngine(tracktion, provenance),
      pluginHost(tracktion, projectEngine,
                 pluginCacheFile == juce::File() ? PluginScanner::defaultCacheFile()
                                                  : std::move(pluginCacheFile),
                 showPluginWindows), hardwareRecordingInput(useHardwareRecordingInput)
{
    if (hardwareRecordingInput) tracktion.audioDeviceManager().addAudioCallback(&inputRecording);
}

AudioEngine::~AudioEngine()
{
    if (hardwareRecordingInput) tracktion.audioDeviceManager().removeAudioCallback(&inputRecording);
    inputRecording.cancelRecording();
}

std::vector<AudioInputChoice> AudioEngine::recordingInputs()
{
    if (! hardwareRecordingInput) return { { "Synthetic input", 0, "Input 1" } };
    return tracktion.recordingInputs();
}

juce::Result AudioEngine::setTrackRecordingInput(int index, const AudioInputChoice& choice)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Stop or cancel the take before changing input");
    const auto* project = projectEngine.currentProject();
    if (! project || ! juce::isPositiveAndBelow(index, static_cast<int>(project->tracks.size()))
        || project->tracks[static_cast<std::size_t>(index)].type != TrackType::audio)
        return juce::Result::fail("Only Audio tracks accept audio recording input");
    const auto available = recordingInputs();
    const auto selected = std::find_if(available.begin(), available.end(), [&](const auto& input)
        { return input.deviceName == choice.deviceName && input.channelIndex == choice.channelIndex; });
    if (selected == available.end())
        return juce::Result::fail("The selected audio input is unavailable");
    const auto wasArmed = armedRecordingTrack >= 0;
    if (wasArmed) (void) setTrackRecordArmed(armedRecordingTrack, false);
    recordingInput = *selected; // metadata comes from enumeration, not caller-supplied labels
    return wasArmed ? setTrackRecordArmed(index, true) : juce::Result::ok();
}

juce::Result AudioEngine::setTrackRecordArmed(int index, bool enabled)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Stop or cancel the take before changing arm");
    const auto* project = projectEngine.currentProject();
    if (! project || ! juce::isPositiveAndBelow(index, static_cast<int>(project->tracks.size()))
        || project->tracks[static_cast<std::size_t>(index)].type != TrackType::audio)
        return juce::Result::fail("Only Audio tracks can be armed for audio recording");
    inputRecording.setArmed(false);
    armedRecordingTrack = -1;
    if (! enabled)
    {
        if (hardwareRecordingInput) tracktion.disableRecordingInput();
        return juce::Result::ok();
    }
    if (recordingInput.deviceName.isEmpty())
    {
        const auto choices = recordingInputs();
        if (choices.empty()) return juce::Result::fail("No audio input available; connect an input and check microphone permission");
        recordingInput = choices.front();
    }
    if (hardwareRecordingInput)
    {
        if (auto result = tracktion.enableRecordingInput(recordingInput); result.failed())
        { tracktion.disableRecordingInput(); return result; }
    }
    else inputRecording.prepareInput(48000.0, true);
    armedRecordingTrack = index;
    inputRecording.setArmed(true);
    return juce::Result::ok();
}

void AudioEngine::processSyntheticRecordingInput(const float* mono, int count) noexcept
{
    if (! hardwareRecordingInput) inputRecording.processInput(mono, count);
}

juce::Result AudioEngine::startAudioRecording()
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Only one take may record at a time");
    if (! provenance.signingConfigured()) return juce::Result::fail(provenance.signingConfigurationError());
    const auto* project = projectEngine.currentProject();
    if (! project || ! juce::isPositiveAndBelow(armedRecordingTrack, static_cast<int>(project->tracks.size()))
        || project->tracks[static_cast<std::size_t>(armedRecordingTrack)].type != TrackType::audio)
        return juce::Result::fail("Arm an Audio track before recording");
    if (transportSnapshot().looping) return juce::Result::fail("Turn Loop off before recording; loop recording is not supported");
    const auto& track = project->tracks[static_cast<std::size_t>(armedRecordingTrack)];
    recordingProjectId = project->id;
    recordingTrackId = track.id;
    recordingTrackName = track.name;
    recordingTakeId = juce::Uuid().toString();
    recordingStartSeconds = transportSnapshot().positionSeconds;
    pluginHost.closeEditorsForOfflineRender();
    if (auto result = inputRecording.startRecording(); result.failed()) return result;
    if (! transportSnapshot().playing) play();
    return juce::Result::ok();
}

juce::Result AudioEngine::stopAudioRecording()
{
    if (! isAudioRecording() || finalizingRecording.exchange(true)) return juce::Result::fail("No active take to finalize");
    RecordedAudioTake take;
    auto result = inputRecording.stopRecording(take);
    tracktion::engine::callBlocking([&] { tracktion.pause(); });
    if (result.wasOk())
    {
        auto* capture = new juce::DynamicObject();
        capture->setProperty("takeId", recordingTakeId);
        capture->setProperty("trackName", recordingTrackName);
        capture->setProperty("inputName", recordingInput.label());
        capture->setProperty("inputIndex", recordingInput.channelIndex);
        capture->setProperty("sampleRate", take.sampleRate);
        capture->setProperty("channelCount", 1);
        capture->setProperty("bitDepth", 24);
        capture->setProperty("startSeconds", recordingStartSeconds);
        capture->setProperty("durationSeconds", take.durationSeconds());
        capture->setProperty("endSeconds", recordingStartSeconds + take.durationSeconds());
        capture->setProperty("frameCount", take.frames);
        capture->setProperty("recordingMode", "external-audio-input");
        capture->setProperty("captureEncoding", "wav-pcm");
        capture->setProperty("applicationName", juce::String(appInfo::name.data()));
        capture->setProperty("applicationVersion", juce::String(appInfo::version.data()));
        auto* parameters = new juce::DynamicObject();
        parameters->setProperty("c2paseq:audioCapture", juce::var(capture));
        StemProvenanceDescriptor descriptor;
        descriptor.title = juce::File::createLegalFileName(recordingTrackName + " Take") + ".wav";
        descriptor.actions = makeHumanRecordedStemActions(juce::var(parameters));
        const auto signedFile = take.file.getSiblingFile(descriptor.title);
        IngredientInfo validation;
        result = provenance.signStemWav(take.file, signedFile, descriptor, validation);
        if (result.wasOk() && (! validation.c2paPresent || ! validation.assetIntact))
            result = juce::Result::fail("Recorded take failed C2PA integrity validation");
        if (result.wasOk()) tracktion::engine::callBlocking([&]
        { result = projectEngine.importRecordedTake(recordingProjectId, recordingTrackId,
            signedFile, recordingStartSeconds, recordingTakeId); });
    }
    inputRecording.cancelRecording();
    finalizingRecording.store(false);
    return result;
}

void AudioEngine::cancelAudioRecording()
{
    if (recordingFinalizing()) return; // finalization has exclusive ownership
    const auto wasRecording = isAudioRecording();
    inputRecording.cancelRecording();
    if (wasRecording) tracktion.pause();
}

bool AudioEngine::isInitialised() const noexcept
{
    return tracktion.isInitialised();
}

juce::String AudioEngine::status() const
{
    return tracktion.audioDeviceDescription();
}

AudioDeviceSnapshot AudioEngine::audioDeviceSnapshot() const
{
    return tracktion.audioDeviceSnapshot();
}

TransportSnapshot AudioEngine::transportSnapshot() const
{
    return tracktion.transportSnapshot();
}

TrackLevelSnapshot AudioEngine::trackLevelSnapshot(int trackIndex) noexcept
{
    return tracktion.trackLevelSnapshot(trackIndex);
}

juce::AudioDeviceManager& AudioEngine::audioDeviceManager() noexcept
{
    return tracktion.audioDeviceManager();
}

void AudioEngine::play()
{
    tracktion.play();
}

void AudioEngine::pause()
{
    tracktion.pause();
}

void AudioEngine::stop()
{
    tracktion.stop();
}

void AudioEngine::seek(double positionSeconds)
{
    tracktion.seek(positionSeconds);
}

void AudioEngine::setLooping(bool shouldLoop, const juce::String& selectedClipId)
{
    projectEngine.setLooping(shouldLoop, selectedClipId);
}

juce::Result AudioEngine::setLoopRangeAndEnable(double startSeconds, double endSeconds)
{
    return projectEngine.setLoopRangeAndEnable(startSeconds, endSeconds);
}

void AudioEngine::setBpm(double bpm)
{
    tracktion.setBpm(bpm);
    projectEngine.setBpm(bpm);
}

juce::Result AudioEngine::importAudio(const juce::File& source,
                                      int trackIndex,
                                      double startSeconds)
{
    return projectEngine.importAudio(source, trackIndex, startSeconds);
}

juce::Result AudioEngine::createMidiClip(int trackIndex, double startBeats,
                                         double lengthBeats)
{
    return projectEngine.createMidiClip(trackIndex, startBeats, lengthBeats);
}

juce::Result AudioEngine::moveMidiClip(const juce::String& id, int trackIndex,
                                       double startBeats)
{
    return projectEngine.moveMidiClip(id, trackIndex, startBeats);
}

juce::Result AudioEngine::trimMidiClip(const juce::String& id, double startBeats,
                                       double lengthBeats)
{
    return projectEngine.trimMidiClip(id, startBeats, lengthBeats);
}

juce::Result AudioEngine::addMidiNote(const juce::String& clipId, int noteNumber,
                                      double startBeats, double durationBeats, int velocity)
{
    return projectEngine.addMidiNote(clipId, noteNumber, startBeats,
                                     durationBeats, velocity);
}

juce::Result AudioEngine::updateMidiNote(const juce::String& clipId,
                                         const juce::String& noteId,
                                         int noteNumber, double startBeats,
                                         double durationBeats, int velocity)
{
    return projectEngine.updateMidiNote(clipId, noteId, noteNumber,
                                        startBeats, durationBeats, velocity);
}

juce::Result AudioEngine::deleteMidiNote(const juce::String& clipId,
                                         const juce::String& noteId)
{
    return projectEngine.deleteMidiNote(clipId, noteId);
}

juce::Result AudioEngine::insertMidiNotes(
    const juce::String& clipId,
    const std::vector<ArrangementMidiNoteSnapshot>& notes)
{
    return projectEngine.insertMidiNotes(clipId, notes);
}

juce::Result AudioEngine::deleteMidiNotes(
    const juce::String& clipId,
    const std::vector<juce::String>& noteIds)
{
    return projectEngine.deleteMidiNotes(clipId, noteIds);
}

juce::Result AudioEngine::moveClip(const juce::String& id, int trackIndex, double start)
{
    return projectEngine.moveClip(id, trackIndex, start);
}

juce::Result AudioEngine::trimClip(const juce::String& id, double start,
                                   double offset, double length)
{
    return projectEngine.trimClip(id, start, offset, length);
}

juce::Result AudioEngine::deleteClip(const juce::String& id) { return projectEngine.deleteClip(id); }
juce::Result AudioEngine::deleteClips(const std::vector<juce::String>& ids) { return projectEngine.deleteClips(ids); }
juce::Result AudioEngine::duplicateClip(const juce::String& id) { return projectEngine.duplicateClip(id); }
juce::Result AudioEngine::copyClips(const std::vector<juce::String>& ids) { return projectEngine.copyClips(ids); }
juce::Result AudioEngine::cutClips(const std::vector<juce::String>& ids) { return projectEngine.cutClips(ids); }
juce::Result AudioEngine::duplicateClips(const std::vector<juce::String>& ids) { return projectEngine.duplicateClips(ids); }
juce::Result AudioEngine::copyTimeRange(const ArrangementTimeSelection& selection) { return projectEngine.copyTimeRange(selection); }
juce::Result AudioEngine::cutTimeRange(const ArrangementTimeSelection& selection) { return projectEngine.cutTimeRange(selection); }
juce::Result AudioEngine::deleteAudioTimeRange(const ArrangementTimeSelection& selection)
{
    if (isAudioRecording() || recordingFinalizing())
        return juce::Result::fail("Stop recording before deleting an audio range");
    return projectEngine.deleteAudioTimeRange(selection);
}
juce::Result AudioEngine::duplicateTimeRange(const ArrangementTimeSelection& selection) { return projectEngine.duplicateTimeRange(selection); }
juce::Result AudioEngine::pasteClipboard(double destination, int track) { return projectEngine.pasteClipboard(destination, track); }
bool AudioEngine::hasClipboard() const noexcept { return projectEngine.hasClipboard(); }
juce::Result AudioEngine::splitClip(const juce::String& id, double position)
{
    return projectEngine.splitClip(id, position);
}
juce::Result AudioEngine::addAudioTrack() { return projectEngine.addAudioTrack(); }
juce::Result AudioEngine::addMidiTrack() { return projectEngine.addMidiTrack(); }
juce::Result AudioEngine::deleteTrack(int i)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Finish recording before deleting tracks");
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.deleteTrack(i);
}
juce::Result AudioEngine::deleteAudioTrack(int i)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Finish recording before deleting tracks");
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.deleteAudioTrack(i);
}
juce::Result AudioEngine::setTrackName(int i, const juce::String& n) { return projectEngine.setTrackName(i, n); }
juce::Result AudioEngine::setTrackMute(int i, bool v) { return projectEngine.setTrackMute(i, v); }
juce::Result AudioEngine::setTrackSolo(int i, bool v) { return projectEngine.setTrackSolo(i, v); }
juce::Result AudioEngine::setTrackGain(int i, double v) { return projectEngine.setTrackGain(i, v); }
juce::Result AudioEngine::setTrackPan(int i, double v) { return projectEngine.setTrackPan(i, v); }
juce::Result AudioEngine::beginTrackMixGesture(int i) { return projectEngine.beginTrackMixGesture(i); }
juce::Result AudioEngine::previewTrackGain(int i, double v) { return projectEngine.previewTrackGain(i, v); }
juce::Result AudioEngine::previewTrackPan(int i, double v) { return projectEngine.previewTrackPan(i, v); }
juce::Result AudioEngine::endTrackMixGesture(int i) { return projectEngine.endTrackMixGesture(i); }
juce::Result AudioEngine::sendLiveMidiMessage(int i, const juce::MidiMessage& message)
{
    return tracktion.sendLiveMidiMessage(i, message);
}
void AudioEngine::allNotesOff(int i) { tracktion.allNotesOff(i); }
const std::vector<PluginDescriptor>& AudioEngine::availableVst3Plugins() const noexcept
{
    return pluginHost.availablePlugins();
}
juce::Result AudioEngine::scanVst3Plugins(const juce::FileSearchPath& paths)
{
    return pluginHost.scanVst3(paths);
}

juce::Result AudioEngine::scanVst3PluginBundle(const juce::File& bundle)
{
    return pluginHost.scanVst3Bundle(bundle);
}
juce::Result AudioEngine::loadTrackPlugin(int i, const juce::String& id)
{
    return pluginHost.loadTrackPlugin(i, id);
}
juce::Result AudioEngine::setTrackPluginBypassed(int i, bool bypassed)
{
    return pluginHost.setTrackPluginBypassed(i, bypassed);
}
juce::Result AudioEngine::removeTrackPlugin(int i) { return pluginHost.removeTrackPlugin(i); }
juce::Result AudioEngine::openTrackPluginEditor(
    int i, std::function<bool(const juce::KeyPress&)> keyHandler)
{
    return pluginHost.openTrackPluginEditor(i, std::move(keyHandler));
}
bool AudioEngine::undo()
{
    if (isAudioRecording() || recordingFinalizing() || ! projectEngine.canUndo()) return false;
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.undo();
}
bool AudioEngine::redo()
{
    if (isAudioRecording() || recordingFinalizing() || ! projectEngine.canRedo()) return false;
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.redo();
}
bool AudioEngine::canUndo() const noexcept { return projectEngine.canUndo(); }
bool AudioEngine::canRedo() const noexcept { return projectEngine.canRedo(); }
void AudioEngine::setTimelineView(double pixels, double scroll)
{
    projectEngine.setTimelineView(pixels, scroll);
}
double AudioEngine::timelinePixelsPerSecond() const noexcept
{
    const auto* project = projectEngine.currentProject();
    return project != nullptr ? project->timelinePixelsPerSecond : 96.0;
}
double AudioEngine::timelineScrollSeconds() const noexcept
{
    const auto* project = projectEngine.currentProject();
    return project != nullptr ? project->timelineScrollSeconds : 0.0;
}

bool AudioEngine::isSupportedAudioFile(const juce::File& file)
{
    return file.hasFileExtension("wav;aif;aiff;mp3");
}

std::vector<ArrangementTrackSnapshot> AudioEngine::arrangementSnapshot() const
{
    return projectEngine.arrangementSnapshot();
}

juce::AudioFormatManager& AudioEngine::audioFormatManager() noexcept
{
    return tracktion.audioFormatManager();
}

juce::AudioThumbnailCache& AudioEngine::audioThumbnailCache() noexcept
{
    return tracktion.audioThumbnailCache();
}

IngredientInfo AudioEngine::inspectProvenance(const juce::File& file) const
{
    return provenance.inspect(file);
}

bool AudioEngine::signingConfigured() const
{
    return provenance.signingConfigured();
}

juce::String AudioEngine::signingCredentialStatus() const
{
    return provenance.signingCredentialStatus();
}

juce::Result AudioEngine::configureSigningCredential(const juce::File& file)
{
    return provenance.configureSigningCredential(file);
}

juce::Result AudioEngine::removeSigningCredential()
{
    return provenance.removeSigningCredential();
}

void AudioEngine::setSoftBindingEnabled(bool enabled) noexcept
{
    useSoftBinding = enabled;
}

bool AudioEngine::softBindingEnabled() const noexcept
{
    return useSoftBinding;
}

bool AudioEngine::watermarkAvailable() const
{
    return watermark->isAvailable();
}

juce::String AudioEngine::watermarkStatus() const
{
    return watermark->statusDescription();
}

void AudioEngine::setFingerprintEnabled(bool enabled) noexcept
{
    useFingerprint = enabled;
}

bool AudioEngine::fingerprintEnabled() const noexcept
{
    return useFingerprint;
}

juce::String AudioEngine::fingerprintStatus() const
{
    return fingerprint->statusDescription();
}

ExportResult AudioEngine::exportMix(const juce::File& destination,
                                    ExportProgressCallback progress,
                                    ExportCancellationCheck shouldCancel)
{
    ExportResult result;
    result.outputFile = destination;
    if (isAudioRecording() || recordingFinalizing())
    { result.result = juce::Result::fail("Finish recording before exporting"); return result; }
    result.result = juce::Result::fail("Create or open a project before exporting");
    std::optional<Project> snapshot;
    std::optional<ProjectPaths> paths;
    // Capture live instrument state on the owner thread into an export-only copy.
    // Do not save, alter the canonical project, or create undo history.
    tracktion::engine::callBlocking([&]
    {
        if (projectEngine.currentProject() == nullptr || projectEngine.currentPaths() == nullptr)
            return;
        Project captured;
        result.result = projectEngine.createExportSnapshot(captured);
        if (result.result.failed()) return;
        snapshot = std::move(captured);
        paths = *projectEngine.currentPaths();
        pluginHost.closeEditorsForOfflineRender();
    });
    if (! snapshot || ! paths) return result;
    ExportController controller(tracktion, provenance, watermark.get(),
                                &softBindingOutbox, useSoftBinding,
                                fingerprint.get(), useFingerprint,
                                std::move(progress), std::move(shouldCancel));
    return controller.exportMix(*snapshot, *paths, destination);
}

juce::Result AudioEngine::createProject(const juce::File& projectFolder,
                                        const juce::String& projectName)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Finish recording before changing project");
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.createProject(projectFolder, projectName);
}

juce::Result AudioEngine::saveProject()
{
    return projectEngine.saveProject();
}

juce::Result AudioEngine::openProject(const juce::File& projectFolder)
{
    if (isAudioRecording() || recordingFinalizing()) return juce::Result::fail("Finish recording before changing project");
    if (armedRecordingTrack >= 0) (void) setTrackRecordArmed(armedRecordingTrack, false);
    return projectEngine.openProject(projectFolder);
}

bool AudioEngine::hasProject() const noexcept
{
    return projectEngine.hasProject();
}

juce::String AudioEngine::projectName() const
{
    return projectEngine.displayName();
}
}
