#include "AudioEngine.h"

#include "export/ExportController.h"

namespace c2paseq
{
juce::Result AudioEngine::prepareMidiStem(int trackIndex, MidiStemPlan& plan)
{
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
                         std::unique_ptr<FingerprintService> fingerprintService)
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
                 showPluginWindows)
{
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
juce::Result AudioEngine::duplicateTimeRange(const ArrangementTimeSelection& selection) { return projectEngine.duplicateTimeRange(selection); }
juce::Result AudioEngine::pasteClipboard(double destination, int track) { return projectEngine.pasteClipboard(destination, track); }
bool AudioEngine::hasClipboard() const noexcept { return projectEngine.hasClipboard(); }
juce::Result AudioEngine::splitClip(const juce::String& id, double position)
{
    return projectEngine.splitClip(id, position);
}
juce::Result AudioEngine::addAudioTrack() { return projectEngine.addAudioTrack(); }
juce::Result AudioEngine::addMidiTrack() { return projectEngine.addMidiTrack(); }
juce::Result AudioEngine::deleteTrack(int i) { return projectEngine.deleteTrack(i); }
juce::Result AudioEngine::deleteAudioTrack(int i) { return projectEngine.deleteAudioTrack(i); }
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
bool AudioEngine::undo() { return projectEngine.undo(); }
bool AudioEngine::redo() { return projectEngine.redo(); }
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
    return projectEngine.createProject(projectFolder, projectName);
}

juce::Result AudioEngine::saveProject()
{
    return projectEngine.saveProject();
}

juce::Result AudioEngine::openProject(const juce::File& projectFolder)
{
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
