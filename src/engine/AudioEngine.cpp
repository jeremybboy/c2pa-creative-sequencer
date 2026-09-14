#include "AudioEngine.h"

#include "export/ExportController.h"

namespace c2paseq
{
AudioEngine::AudioEngine()
    : projectEngine(tracktion, provenance)
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

void AudioEngine::setLooping(bool shouldLoop)
{
    tracktion.setLooping(shouldLoop);
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
juce::Result AudioEngine::duplicateClip(const juce::String& id) { return projectEngine.duplicateClip(id); }
juce::Result AudioEngine::splitClip(const juce::String& id, double position)
{
    return projectEngine.splitClip(id, position);
}
juce::Result AudioEngine::setTrackName(int i, const juce::String& n) { return projectEngine.setTrackName(i, n); }
juce::Result AudioEngine::setTrackMute(int i, bool v) { return projectEngine.setTrackMute(i, v); }
juce::Result AudioEngine::setTrackSolo(int i, bool v) { return projectEngine.setTrackSolo(i, v); }
juce::Result AudioEngine::setTrackGain(int i, double v) { return projectEngine.setTrackGain(i, v); }
juce::Result AudioEngine::setTrackPan(int i, double v) { return projectEngine.setTrackPan(i, v); }
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

ExportResult AudioEngine::exportMix(const juce::File& destination)
{
    if (const auto* project = projectEngine.currentProject())
        if (const auto* paths = projectEngine.currentPaths())
        {
            ExportController controller(tracktion, provenance);
            return controller.exportMix(*project, *paths, destination);
        }

    ExportResult result;
    result.outputFile = destination;
    result.result = juce::Result::fail("Create or open a project before exporting");
    return result;
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
