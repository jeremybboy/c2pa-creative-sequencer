# MIDI v1 staged sequence

PR 018 delivered the MIDI data, arrangement, and piano-roll foundation. PR 019 is the separately
reviewable instrument-playback and rendering slice. Later input and recording work remains separate
so each dependency can be built, tested, and accepted before the next begins.

Status legend: **COMPLETE** = implemented and verified; **CURRENT** = next authorized checkpoint;
**PLANNED** = not implemented and must not be presented as working.

## Checkpoint 1 — Foundation — COMPLETE

- Explicit Audio/MIDI track types with stable identity.
- Audio clips remain second-based; MIDI clips and notes are beat-based.
- MIDI note pitch, start, duration, and velocity persist in project schema 2.
- Legacy schema-1 tracks migrate to Audio.
- Audio/MIDI track creation, rename, delete, undo/redo, and save/reopen.

## Checkpoint 2 — MIDI Clip + Piano Roll Editing — COMPLETE

- Create MIDI clips from an exact arrangement time selection and open a piano keyboard, absolute
  song-bar ruler, beat grid, and velocity lane.
- Draw, select, move, resize, delete, and velocity-edit notes.
- Copy, paste, and duplicate notes; inserted notes become the active selection so repeated Command-D advances.
- Move, trim, copy, paste, and duplicate MIDI clips in the arrangement; duplicated clips likewise become the active selection.
- Provide live drag feedback and pointer-dependent trackpad pinch zoom for time or pitch range.
- Show the arrangement playhead in the piano roll while transport crosses the open MIDI clip.

## Checkpoint 3 — PR 019 VST3 Instrument Playback + Rendering — CURRENT

- Allow one VST3 instrument on a MIDI track and schedule project MIDI notes into Tracktion.
- Use a deterministic test synthesizer for realtime audible playback.
- Persist instrument identity/state, including live parameter changes across editor close, arrangement rebuild, save, and reopen, while keeping existing Audio effects behavior unchanged.
- Render arranged instrument output through the normal offline audio mix and existing signed export.
- Loading a different VST3 instrument replaces the track's single existing slot.

## Checkpoint 4 — Live Laptop Keyboard + Monitoring — PLANNED

- Map laptop-keyboard input to MIDI note on/off with octave controls.
- Route live notes to the selected or armed MIDI track while transport runs.
- Add focus guards, stuck-note prevention, and all-notes-off safety.

## Checkpoint 5 — MIDI Recording — PLANNED

- Add record arm and capture incoming note events while transport runs.
- Reconstruct note durations and commit the performance as one beat-based, undoable MIDI clip.
- Support immediate playback, note editing, and save/reopen.

Audio recording, external MIDI hardware, tempo maps, CLAP, MPE, automation, warping, time
stretching, advanced routing, and full-DAW expansion remain outside PR 018.
