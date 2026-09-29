#include "engine/AudioEngine.h"

#include <algorithm>
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
    if (tracks.size() != 4
        || std::any_of(tracks.begin(), tracks.end(), [](const auto& track)
            { return track.type != c2paseq::TrackType::audio; }))
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
    if (engine.duplicateClips({ duplicateId }).failed())
        return fail(11, "repeated duplicate failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[1].clips.size() != 3
        || ! close(tracks[1].clips[2].startSeconds, 7.25))
        return fail(11, "repeated duplicate did not advance to the next section");
    const auto repeatedDuplicateId = tracks[1].clips[2].id;

    if (engine.splitClip(originalId, 5.0).failed()
        || engine.deleteClips({ duplicateId, repeatedDuplicateId }).failed())
        return fail(12, "split or delete failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[1].clips.size() != 2)
        return fail(13, "split/delete produced the wrong clip count");

    engine.setLooping(true, originalId);
    const auto selectedLoop = engine.transportSnapshot();
    if (! selectedLoop.looping || ! close(selectedLoop.loopStartSeconds, 4.25)
        || ! close(selectedLoop.loopEndSeconds, 5.0))
        return fail(14, "selected clip did not configure the live transport loop");
    engine.setLooping(false);
    if (engine.setLoopRangeAndEnable(1.25, 2.75).failed())
        return fail(14, "time-selection loop could not be enabled");
    const auto timeSelectionLoop = engine.transportSnapshot();
    if (! timeSelectionLoop.looping || ! close(timeSelectionLoop.loopStartSeconds, 1.25)
        || ! close(timeSelectionLoop.loopEndSeconds, 2.75))
        return fail(14, "time-selection loop range was not exact");
    engine.setLooping(false);

    engine.seek(0.5);
    engine.play();
    const auto beforeAudibilityChange = engine.transportSnapshot();
    if (! beforeAudibilityChange.playing || beforeAudibilityChange.positionSeconds < 0.49)
        return fail(14, "transport test could not start playback from the test position");
    if (engine.trimClip(originalId, 4.25, 0.25, 0.75).failed())
        return fail(14, "live clip edit failed");
    juce::Thread::sleep(100);
    const auto afterClipEdit = engine.transportSnapshot();
    if (! afterClipEdit.playing || afterClipEdit.positionSeconds < 0.49)
        return fail(14, "editing a clip stopped playback or reset the playhead");
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

    if (engine.beginTrackMixGesture(1).failed()
        || engine.previewTrackGain(1, -3.0).failed()
        || engine.previewTrackGain(1, -6.0).failed())
        return fail(14, "live gain gesture failed");
    const auto duringGain = engine.transportSnapshot();
    if (! duringGain.playing || duringGain.positionSeconds < 0.49)
        return fail(14, "gain preview stopped playback or reset the playhead");
    if (engine.endTrackMixGesture(1).failed())
        return fail(14, "live gain gesture could not commit");
    const auto afterGain = engine.transportSnapshot();
    if (! afterGain.playing || afterGain.positionSeconds < 0.49)
        return fail(14, "gain commit stopped playback or reset the playhead");
    engine.pause();
    tracks = engine.arrangementSnapshot();
    if (! close(tracks[1].gainDb, -6.0) || ! engine.undo())
        return fail(15, "gain gesture did not create one undoable edit");
    tracks = engine.arrangementSnapshot();
    if (! close(tracks[1].gainDb, 0.0) || ! engine.redo())
        return fail(16, "gain undo/redo did not restore the live value");

    engine.seek(0.5);
    engine.play();
    if (engine.beginTrackMixGesture(1).failed()
        || engine.previewTrackPan(1, -0.2).failed()
        || engine.previewTrackPan(1, 0.4).failed())
        return fail(16, "live pan gesture failed");
    const auto duringPan = engine.transportSnapshot();
    if (! duringPan.playing || duringPan.positionSeconds < 0.49)
        return fail(16, "pan preview stopped playback or reset the playhead");
    if (engine.endTrackMixGesture(1).failed())
        return fail(16, "live pan gesture could not commit");
    const auto afterPan = engine.transportSnapshot();
    if (! afterPan.playing || afterPan.positionSeconds < 0.49)
        return fail(16, "pan commit stopped playback or reset the playhead");
    engine.pause();
    tracks = engine.arrangementSnapshot();
    if (! close(tracks[1].pan, 0.4) || ! engine.undo())
        return fail(16, "pan gesture did not create one undoable edit");
    tracks = engine.arrangementSnapshot();
    if (! close(tracks[1].pan, 0.0) || ! engine.redo())
        return fail(16, "pan undo/redo did not restore the live value");

    if (engine.setTrackName(1, "Vocal").failed())
        return fail(14, "track control mutation failed");

    engine.setTimelineView(144.0, 8.0);
    engine.setLooping(true);
    if (engine.saveProject().failed() || engine.openProject(projectFolder).failed())
        return fail(17, "save/reopen failed");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 4 || tracks[1].name != "Vocal" || ! tracks[1].muted
        || ! tracks[1].soloed || ! close(tracks[1].gainDb, -6.0)
        || ! close(tracks[1].pan, 0.4) || tracks[1].clips.size() != 2
        || ! close(engine.timelinePixelsPerSecond(), 144.0)
        || ! close(engine.timelineScrollSeconds(), 8.0))
        return fail(18, "saved arrangement did not restore exactly");
    const auto restoredLoop = engine.transportSnapshot();
    if (! restoredLoop.looping || ! close(restoredLoop.loopStartSeconds, 0.0)
        || ! close(restoredLoop.loopEndSeconds, 5.75))
        return fail(18, "full-arrangement loop did not survive save/reopen");

    const auto& left = tracks[1].clips[0];
    const auto& right = tracks[1].clips[1];
    if (left.id != originalId || ! close(left.startSeconds, 4.25)
        || ! close(left.sourceOffsetSeconds, 0.25) || ! close(left.lengthSeconds, 0.75)
        || ! close(right.startSeconds, 5.0) || ! close(right.sourceOffsetSeconds, 1.0)
        || ! close(right.lengthSeconds, 0.75))
        return fail(19, "split clip timing or source offsets did not survive reopen");

    engine.seek(0.5);
    engine.play();
    if (engine.addAudioTrack().failed())
        return fail(20, "could not add an audio track");
    juce::Thread::sleep(100);
    const auto afterAudioTrackAdd = engine.transportSnapshot();
    if (! afterAudioTrackAdd.playing || afterAudioTrackAdd.positionSeconds < 0.49)
        return fail(20, "adding an audio track stopped playback or reset the playhead");
    engine.pause();
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks.back().name != "Audio 5")
        return fail(21, "added track did not receive stable default state");
    const auto addedTrackId = tracks.back().id;
    if (engine.importAudio(source, 4, 1.0).failed()
        || engine.saveProject().failed() || engine.openProject(projectFolder).failed())
        return fail(22, "added track could not accept audio or survive save/reopen");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks.back().id != addedTrackId
        || tracks.back().clips.size() != 1)
        return fail(23, "added track identity or clip did not survive save/reopen");

    if (engine.deleteAudioTrack(4).failed() || engine.arrangementSnapshot().size() != 4)
        return fail(24, "track deletion did not remove the populated track");
    if (! engine.undo())
        return fail(25, "track deletion could not be undone");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks.back().id != addedTrackId
        || tracks.back().clips.size() != 1 || ! engine.redo())
        return fail(26, "undo/redo did not restore deleted track content exactly");
    if (engine.arrangementSnapshot().size() != 4
        || engine.deleteAudioTrack(99).wasOk())
        return fail(27, "invalid track deletion changed the arrangement");

    engine.seek(0.5);
    engine.play();
    if (engine.addMidiTrack().failed())
        return fail(28, "could not add a MIDI track");
    juce::Thread::sleep(100);
    const auto afterMidiTrackAdd = engine.transportSnapshot();
    if (! afterMidiTrackAdd.playing || afterMidiTrackAdd.positionSeconds < 0.49)
        return fail(28, "adding a MIDI track stopped playback or reset the playhead");
    engine.pause();
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks.back().type != c2paseq::TrackType::midi
        || tracks.back().name != "MIDI 1" || ! tracks.back().clips.empty())
        return fail(28, "MIDI and Audio tracks did not coexist with explicit types");
    const auto midiTrackId = tracks.back().id;
    if (engine.setTrackName(4, "Keys").failed()
        || engine.importAudio(source, 4, 0.0).wasOk()
        || engine.setTrackMute(4, true).wasOk()
        || engine.setTrackGain(4, -3.0).wasOk())
        return fail(28, "MIDI foundation exposed unsupported audio behavior");
    if (engine.saveProject().failed() || engine.openProject(projectFolder).failed())
        return fail(28, "MIDI track did not survive save/reopen");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks.back().id != midiTrackId
        || tracks.back().name != "Keys"
        || tracks.back().type != c2paseq::TrackType::midi)
        return fail(28, "MIDI track identity, order, name, or type changed on reopen");
    if (engine.deleteTrack(4).failed() || engine.arrangementSnapshot().size() != 4
        || ! engine.undo() || engine.arrangementSnapshot().size() != 5
        || engine.arrangementSnapshot().back().id != midiTrackId
        || ! engine.redo() || engine.arrangementSnapshot().size() != 4)
        return fail(28, "MIDI track deletion did not obey project undo/redo safety");

    if (engine.deleteAudioTrack(3).failed() || engine.deleteAudioTrack(2).failed()
        || engine.deleteAudioTrack(1).failed()
        || engine.arrangementSnapshot().size() != 1
        || engine.deleteAudioTrack(0).wasOk()
        || engine.arrangementSnapshot().size() != 1)
        return fail(28, "the final audio track was not protected from deletion");

    const auto clipboardProject = temporary.root.getChildFile("Clipboard.c2paseq");
    if (engine.createProject(clipboardProject, "Clipboard").failed()
        || engine.importAudio(source, 0, 0.0).failed())
        return fail(29, "could not create clipboard fixture");
    tracks = engine.arrangementSnapshot();
    const auto clipboardSourceId = tracks[0].clips[0].id;
    const auto clipboardMediaId = tracks[0].clips[0].mediaId;
    const auto range = c2paseq::ArrangementTimeSelection::between(0.5, 1.25, 0, 0);

    if (engine.copyTimeRange(range).failed() || ! engine.hasClipboard()
        || engine.pasteClipboard(3.0, 0).failed())
        return fail(30, "partial-range copy or paste failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[0].clips.size() != 2)
        return fail(31, "partial paste produced the wrong clip count");
    const auto pasted = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [](const auto& clip) { return close(clip.startSeconds, 3.0); });
    if (pasted == tracks[0].clips.end() || pasted->mediaId != clipboardMediaId
        || ! close(pasted->sourceOffsetSeconds, 0.5)
        || ! close(pasted->lengthSeconds, 0.75))
        return fail(32, "partial copy did not preserve media/source range");
    const auto pastedId = pasted->id;
    if (! engine.undo() || engine.arrangementSnapshot()[0].clips.size() != 1
        || ! engine.redo() || engine.arrangementSnapshot()[0].clips.size() != 2)
        return fail(33, "paste was not one undoable action");

    if (engine.cutTimeRange(range).failed())
        return fail(34, "partial cut failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[0].clips.size() != 3)
        return fail(35, "partial cut did not leave two surviving pieces");
    const auto cutLeft = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [](const auto& clip) { return close(clip.startSeconds, 0.0); });
    const auto cutRight = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [](const auto& clip) { return close(clip.startSeconds, 1.25); });
    const auto later = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [](const auto& clip) { return close(clip.startSeconds, 3.0); });
    if (cutLeft == tracks[0].clips.end() || cutRight == tracks[0].clips.end()
        || later == tracks[0].clips.end() || ! close(cutLeft->lengthSeconds, 0.5)
        || ! close(cutRight->sourceOffsetSeconds, 1.25)
        || ! close(cutRight->lengthSeconds, 0.75)
        || later->id != pastedId)
        return fail(36, "cut did not preserve the gap, survivors, or later material");
    if (! engine.undo() || engine.arrangementSnapshot()[0].clips.size() != 2
        || ! engine.redo() || engine.arrangementSnapshot()[0].clips.size() != 3)
        return fail(37, "cut was not one undoable action");

    if (! engine.undo())
        return fail(38, "could not restore pre-cut arrangement");
    const auto duplicateRange = c2paseq::ArrangementTimeSelection::between(0.25, 0.5, 0, 0);
    if (engine.duplicateTimeRange(duplicateRange).failed())
        return fail(39, "partial duplicate failed");
    tracks = engine.arrangementSnapshot();
    const auto fragment = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [&](const auto& clip)
        {
            return clip.id != clipboardSourceId && clip.id != pastedId
                && close(clip.startSeconds, 0.5);
        });
    if (fragment == tracks[0].clips.end() || fragment->mediaId != clipboardMediaId
        || ! close(fragment->sourceOffsetSeconds, 0.25)
        || ! close(fragment->lengthSeconds, 0.25))
        return fail(40, "partial duplicate did not preserve its source range");
    if (! engine.undo() || engine.arrangementSnapshot()[0].clips.size() != 2
        || ! engine.redo() || engine.arrangementSnapshot()[0].clips.size() != 3)
        return fail(41, "duplicate was not one undoable action");

    if (engine.copyClips({ pastedId }).failed() || engine.pasteClipboard(5.0, 0).failed())
        return fail(42, "whole-clip clipboard failed");
    tracks = engine.arrangementSnapshot();
    const auto wholePaste = std::find_if(tracks[0].clips.begin(), tracks[0].clips.end(),
        [](const auto& clip) { return close(clip.startSeconds, 5.0); });
    if (wholePaste == tracks[0].clips.end() || wholePaste->mediaId != clipboardMediaId
        || ! close(wholePaste->sourceOffsetSeconds, 0.5)
        || ! close(wholePaste->lengthSeconds, 0.75))
        return fail(43, "whole-clip paste changed clip properties");

    const auto midiProject = temporary.root.getChildFile("MidiEditing.c2paseq");
    if (engine.createProject(midiProject, "Midi Editing").failed()
        || engine.addMidiTrack().failed())
        return fail(44, "could not create MIDI editing fixture");
    if (engine.createMidiClip(0, 0.0, 4.0).wasOk()
        || engine.createMidiClip(4, 4.0, 8.0).failed())
        return fail(45, "MIDI clip track validation or creation failed");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks[4].midiClips.size() != 1)
        return fail(46, "MIDI clip was not exposed in the arrangement snapshot");
    const auto midiClipId = tracks[4].midiClips[0].id;
    if (engine.createMidiClip(4, 24.0).failed())
        return fail(60, "default MIDI clip creation failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips.size() != 2
        || ! close(tracks[4].midiClips[1].lengthBeats, 16.0)
        || engine.deleteClip(tracks[4].midiClips[1].id).failed())
        return fail(61, "default MIDI clip was not four bars or could not be removed");
    if (engine.createMidiClip(4, 2.5, 5.5).failed())
        return fail(68, "time-selection MIDI clip creation failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips.size() != 2
        || ! close(tracks[4].midiClips[1].startBeats, 2.5)
        || ! close(tracks[4].midiClips[1].lengthBeats, 5.5)
        || engine.deleteClip(tracks[4].midiClips[1].id).failed())
        return fail(69, "MIDI clip did not preserve the exact selected beat range");
    if (engine.addMidiNote(midiClipId, 60, 0.0, 1.0, 90).failed()
        || engine.addMidiNote(midiClipId, 64, 1.0, 2.0, 100).failed()
        || engine.addMidiNote(midiClipId, 67, 6.5, 1.5, 110).failed()
        || engine.addMidiNote(midiClipId, 60, 7.5, 1.0, 100).wasOk())
        return fail(47, "MIDI note creation or clip-bound validation failed");
    tracks = engine.arrangementSnapshot();
    const auto firstNoteId = tracks[4].midiClips[0].notes[0].id;
    if (engine.updateMidiNote(midiClipId, firstNoteId, 61, 0.5, 0.75, 72).failed())
        return fail(48, "MIDI note update failed");
    tracks = engine.arrangementSnapshot();
    const auto& editedNote = tracks[4].midiClips[0].notes[0];
    if (editedNote.noteNumber != 61 || ! close(editedNote.startBeats, 0.5)
        || ! close(editedNote.durationBeats, 0.75) || editedNote.velocity != 72)
        return fail(49, "MIDI note pitch, timing, duration, or velocity was incorrect");

    std::vector<c2paseq::ArrangementMidiNoteSnapshot> pastedNotes {
        { {}, 72, 3.0, 0.5, 88 },
        { {}, 74, 4.0, 1.0, 96 }
    };
    if (engine.insertMidiNotes(midiClipId, pastedNotes).failed())
        return fail(62, "MIDI note clipboard batch insert failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips[0].notes.size() != 5)
        return fail(63, "MIDI note clipboard insert count was incorrect");
    const std::vector<juce::String> pastedNoteIds {
        tracks[4].midiClips[0].notes[3].id,
        tracks[4].midiClips[0].notes[4].id
    };
    if (engine.deleteMidiNotes(midiClipId, pastedNoteIds).failed()
        || engine.arrangementSnapshot()[4].midiClips[0].notes.size() != 3
        || ! engine.undo()
        || engine.arrangementSnapshot()[4].midiClips[0].notes.size() != 5
        || ! engine.redo()
        || engine.arrangementSnapshot()[4].midiClips[0].notes.size() != 3)
        return fail(64, "MIDI multi-note delete was not one undoable gesture");

    if (engine.trimMidiClip(midiClipId, 5.0, 4.0).failed()
        || engine.moveMidiClip(midiClipId, 0, 8.0).wasOk()
        || engine.moveMidiClip(midiClipId, 4, 8.0).failed())
        return fail(50, "MIDI trim or MIDI-track-only move failed");
    tracks = engine.arrangementSnapshot();
    const auto& trimmed = tracks[4].midiClips[0];
    if (! close(trimmed.startBeats, 8.0) || ! close(trimmed.lengthBeats, 4.0)
        || trimmed.notes.size() != 2 || ! close(trimmed.notes[0].startBeats, 0.0)
        || ! close(trimmed.notes[0].durationBeats, 0.25)
        || ! close(trimmed.notes[1].startBeats, 0.0)
        || ! close(trimmed.notes[1].durationBeats, 2.0))
        return fail(51, "MIDI trim did not non-destructively retain intersecting notes");

    const auto beatsBeforeTempoChange = trimmed;
    engine.setBpm(90.0);
    tracks = engine.arrangementSnapshot();
    const auto& afterTempoChange = tracks[4].midiClips[0];
    if (! close(afterTempoChange.startBeats, beatsBeforeTempoChange.startBeats)
        || ! close(afterTempoChange.lengthBeats, beatsBeforeTempoChange.lengthBeats)
        || afterTempoChange.notes.size() != beatsBeforeTempoChange.notes.size()
        || ! close(afterTempoChange.notes[0].startBeats,
                   beatsBeforeTempoChange.notes[0].startBeats)
        || ! close(afterTempoChange.notes[0].durationBeats,
                   beatsBeforeTempoChange.notes[0].durationBeats))
        return fail(65, "tempo change altered beat-based MIDI arrangement data");
    engine.setBpm(120.0);

    engine.setLooping(true, midiClipId);
    const auto midiLoop = engine.transportSnapshot();
    if (! midiLoop.looping || ! close(midiLoop.loopStartSeconds, 4.0)
        || ! close(midiLoop.loopEndSeconds, 6.0))
        return fail(52, "MIDI clip selection did not configure the beat-derived loop");
    engine.setLooping(false);

    if (engine.duplicateClip(midiClipId).failed())
        return fail(66, "MIDI clip duplicate failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips.size() != 2
        || ! close(tracks[4].midiClips[1].startBeats, 12.0)
        || tracks[4].midiClips[1].id == midiClipId
        || tracks[4].midiClips[1].notes[0].id == tracks[4].midiClips[0].notes[0].id
        || ! engine.undo() || engine.arrangementSnapshot()[4].midiClips.size() != 1)
        return fail(67, "MIDI duplicate timing, identity, or undo behavior was incorrect");

    if (engine.copyClips({ midiClipId }).failed()
        || engine.pasteClipboard(8.0, 4).failed())
        return fail(53, "MIDI whole-clip copy or paste failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips.size() != 2
        || close(tracks[4].midiClips[1].startBeats, tracks[4].midiClips[0].startBeats)
        || tracks[4].midiClips[1].id == midiClipId
        || tracks[4].midiClips[1].notes[0].id == tracks[4].midiClips[0].notes[0].id)
        return fail(54, "MIDI paste did not preserve content with fresh identities");

    const auto retainedNoteId = tracks[4].midiClips[0].notes[0].id;
    if (engine.deleteMidiNote(midiClipId, retainedNoteId).failed())
        return fail(55, "MIDI note delete failed");
    tracks = engine.arrangementSnapshot();
    if (tracks[4].midiClips[0].notes.size() != 1 || ! engine.undo())
        return fail(56, "MIDI note delete was not applied or undoable");
    if (engine.arrangementSnapshot()[4].midiClips[0].notes.size() != 2 || ! engine.redo()
        || engine.arrangementSnapshot()[4].midiClips[0].notes.size() != 1)
        return fail(57, "MIDI note delete undo/redo was not exact");

    if (engine.saveProject().failed() || engine.openProject(midiProject).failed())
        return fail(58, "MIDI clip and note save/reopen failed");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 5 || tracks[4].midiClips.size() != 2
        || tracks[4].midiClips[0].notes.size() != 1
        || ! close(tracks[4].midiClips[0].startBeats, 8.0)
        || tracks[4].midiClips[0].notes[0].noteNumber != 64)
        return fail(59, "MIDI arrangement state changed after save/reopen");

    std::cout << "arrangement editing: import, move, trim, split, duplicate, delete, "
                 "live loop range, mute/solo transport preservation, track controls, undo/redo, "
                 "dynamic Audio/MIDI track add/delete, partial clipboard editing, MIDI clip/note "
                 "editing, velocity, MIDI clipboard, and reopen passed\n";
    return 0;
}
