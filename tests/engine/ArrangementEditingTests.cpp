#include "engine/AudioEngine.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
struct ScopedTestDirectory
{
    ScopedTestDirectory()
        : root(juce::File::getCurrentWorkingDirectory()
                   .getNonexistentChildFile("c2paseq-arrangement-editing", {}, false))
    {
        ready = root.createDirectory().wasOk();
    }

    ~ScopedTestDirectory() { root.deleteRecursively(); }

    juce::File root;
    bool ready = false;
};

bool close(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

bool writeTone(const juce::File& file)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 96000;
    juce::AudioBuffer<float> source(1, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
        source.setSample(0, sample, 0.25f * std::sin(
            juce::MathConstants<double>::twoPi * 330.0 * sample / sampleRate));

    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 1, 16, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(source, 0, sampleCount);
}

int fail(int code, const juce::String& message)
{
    std::cerr << message << '\n';
    return code;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ScopedTestDirectory temporary;
    if (! temporary.ready)
        return fail(1, "could not create arrangement test directory");

    const auto source = temporary.root.getChildFile("tone.wav");
    const auto projectFolder = temporary.root.getChildFile("Editing.c2paseq");
    if (! writeTone(source))
        return fail(2, "could not write arrangement audio fixture");

    c2paseq::AudioEngine engine;
    if (const auto result = engine.createProject(projectFolder, "Editing"); result.failed())
        return fail(3, result.getErrorMessage());

    auto tracks = engine.arrangementSnapshot();
    if (tracks.size() != 4)
        return fail(4, "new project did not create four reusable audio tracks");
    if (engine.moveClip("missing-clip", 10, 1.0).wasOk()
        || engine.arrangementSnapshot().size() != 4)
        return fail(4, "failed edit mutated the arrangement");

    if (const auto result = engine.importAudio(source, 0, 2.0); result.failed())
        return fail(5, result.getErrorMessage());
    tracks = engine.arrangementSnapshot();
    if (tracks[0].clips.size() != 1 || ! close(tracks[0].clips[0].startSeconds, 2.0)
        || ! close(tracks[0].clips[0].lengthSeconds, 2.0))
        return fail(6, "import did not preserve dropped time and source duration");
    const auto originalId = tracks[0].clips[0].id;

    if (engine.moveClip(originalId, 1, 4.0).failed()
        || engine.trimClip(originalId, 4.25, 0.25, 1.5).failed())
        return fail(7, "move or trim failed");
    tracks = engine.arrangementSnapshot();
    if (! tracks[0].clips.empty() || tracks[1].clips.size() != 1
        || ! close(tracks[1].clips[0].startSeconds, 4.25)
        || ! close(tracks[1].clips[0].sourceOffsetSeconds, 0.25)
        || ! close(tracks[1].clips[0].lengthSeconds, 1.5))
        return fail(8, "move or trim state was incorrect");

    if (engine.duplicateClip(originalId).failed())
        return fail(9, "duplicate failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[1].clips.size() != 2)
        return fail(10, "duplicate did not create a second clip");
    const auto duplicateId = tracks[1].clips[1].id;
    if (duplicateId == originalId || ! close(tracks[1].clips[1].startSeconds, 5.75))
        return fail(11, "duplicate identity or placement was incorrect");

    if (engine.splitClip(originalId, 5.0).failed()
        || engine.deleteClip(duplicateId).failed())
        return fail(12, "split or delete failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[1].clips.size() != 2)
        return fail(13, "split/delete produced the wrong clip count");

    engine.seek(0.5);
    engine.play();
    const auto beforeAudibilityChange = engine.transportSnapshot();
    if (! beforeAudibilityChange.playing || beforeAudibilityChange.positionSeconds < 0.49)
        return fail(14, "transport test could not start playback from the test position");
    if (engine.setTrackMute(1, true).failed())
        return fail(14, "live mute failed");
    const auto afterMute = engine.transportSnapshot();
    if (! afterMute.playing || afterMute.positionSeconds < 0.49)
        return fail(14, "mute stopped playback or reset the playhead");
    if (engine.setTrackSolo(1, true).failed())
        return fail(14, "live solo failed");
    const auto afterSolo = engine.transportSnapshot();
    if (! afterSolo.playing || afterSolo.positionSeconds < 0.49)
        return fail(14, "solo stopped playback or reset the playhead");
    engine.pause();

    if (engine.setTrackName(1, "Vocal").failed()
        || engine.setTrackGain(1, -6.0).failed()
        || engine.setTrackPan(1, 0.4).failed())
        return fail(14, "track control mutation failed");
    if (! engine.undo())
        return fail(15, "undo failed");
    tracks = engine.arrangementSnapshot();
    if (! close(tracks[1].pan, 0.0) || ! engine.redo())
        return fail(16, "undo/redo did not restore track pan");

    engine.setTimelineView(144.0, 8.0);
    if (engine.saveProject().failed() || engine.openProject(projectFolder).failed())
        return fail(17, "save/reopen failed");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 4 || tracks[1].name != "Vocal" || ! tracks[1].muted
        || ! tracks[1].soloed || ! close(tracks[1].gainDb, -6.0)
        || ! close(tracks[1].pan, 0.4) || tracks[1].clips.size() != 2
        || ! close(engine.timelinePixelsPerSecond(), 144.0)
        || ! close(engine.timelineScrollSeconds(), 8.0))
        return fail(18, "saved arrangement did not restore exactly");

    const auto& left = tracks[1].clips[0];
    const auto& right = tracks[1].clips[1];
    if (left.id != originalId || ! close(left.startSeconds, 4.25)
        || ! close(left.sourceOffsetSeconds, 0.25) || ! close(left.lengthSeconds, 0.75)
        || ! close(right.startSeconds, 5.0) || ! close(right.sourceOffsetSeconds, 1.0)
        || ! close(right.lengthSeconds, 0.75))
        return fail(19, "split clip timing or source offsets did not survive reopen");

    std::cout << "arrangement editing: import, move, trim, split, duplicate, delete, "
                 "live mute/solo transport preservation, track controls, undo/redo, "
                 "and reopen passed\n";
    return 0;
}
