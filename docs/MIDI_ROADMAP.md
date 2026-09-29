# MIDI v1 staged sequence

PR 018 delivered the MIDI data, arrangement, and piano-roll foundation. PR 019 delivered the
instrument-playback and rendering slice. PR 020 is the separately reviewable live computer-keyboard
monitoring slice; recording remains separate so it cannot be confused with transient monitoring.

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

## Checkpoint 3 — PR 019 VST3 Instrument Playback + Rendering — COMPLETE

- Allow one VST3 instrument on a MIDI track and schedule project MIDI notes into Tracktion.
- Use a deterministic test synthesizer for realtime audible playback.
- Persist instrument identity/state, including live parameter changes across editor close, arrangement rebuild, save, and reopen, while keeping existing Audio effects behavior unchanged.
- Render arranged instrument output through the normal offline audio mix and existing signed export.
- Loading a different VST3 instrument replaces the track's single existing slot.

## Checkpoint 4 — PR 020 Live Computer Keyboard + Monitoring — CURRENT

- Require explicit activation through the toolbar keyboard button; selecting a MIDI track alone does
  not start listening.
- Map A–L rows to MIDI note on/off events and use Z/X for octave changes.
- Route transient live notes to the selected MIDI track's single enabled VST3 instrument whether the
  transport is stopped or running, without writing notes or moving transport.
- Stop notes on key release, focus loss, track/instrument changes, export, and keyboard deactivation.

## Checkpoint 5 — MIDI Recording — PLANNED

- Add record arm and capture incoming note events while transport runs.
- Reconstruct note durations and commit the performance as one beat-based, undoable MIDI clip.
- Support immediate playback, note editing, and save/reopen.

Audio recording, external MIDI hardware, tempo maps, CLAP, MPE, automation, warping, time
stretching, advanced routing, and full-DAW expansion remain outside MIDI v1.
