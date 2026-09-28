# PR 018 MIDI v1 checkpoint sequence

PR 018 is the umbrella pull request for the complete basic MIDI v1. Work proceeds through the
checkpoint commits below so each dependency can be reviewed and verified before the next begins.
After each checkpoint: build Release, run focused tests and the full normal CTest suite, update the
implementation brief and visual, push, report evidence, and wait for explicit human approval.

Status legend: **COMPLETE** = implemented and verified; **CURRENT** = next authorized checkpoint;
**PLANNED** = not implemented and must not be presented as working.

## Checkpoint 1 — Foundation — COMPLETE

- Explicit Audio/MIDI track types with stable identity.
- Audio clips remain second-based; MIDI clips and notes are beat-based.
- MIDI note pitch, start, duration, and velocity persist in project schema 2.
- Legacy schema-1 tracks migrate to Audio.
- Audio/MIDI track creation, rename, delete, undo/redo, and save/reopen.

## Checkpoint 2 — MIDI Clip + Piano Roll Editing — CURRENT

- Create and open MIDI clips; show a piano keyboard, beat grid, and velocity lane.
- Draw, select, move, resize, delete, and velocity-edit notes.
- Copy, paste, and duplicate notes.
- Move, trim, copy, paste, and duplicate MIDI clips in the arrangement.

## Checkpoint 3 — VST3 Instrument Hosting + MIDI Playback — PLANNED

- Allow one VST3 instrument on a MIDI track and schedule project MIDI notes into Tracktion.
- Use a deterministic test synthesizer for realtime audible playback.
- Persist instrument identity/state and keep existing Audio effects behavior unchanged.

## Checkpoint 4 — Live Laptop Keyboard + Monitoring — PLANNED

- Map laptop-keyboard input to MIDI note on/off with octave controls.
- Route live notes to the selected or armed MIDI track while transport runs.
- Add focus guards, stuck-note prevention, and all-notes-off safety.

## Checkpoint 5 — MIDI Recording — PLANNED

- Add record arm and capture incoming note events while transport runs.
- Reconstruct note durations and commit the performance as one beat-based, undoable MIDI clip.
- Support immediate playback, note editing, and save/reopen.

## Checkpoint 6 — Instrument Rendering + C2PA Export — PLANNED

- Render MIDI instruments through the normal offline audio mix.
- Verify Audio and MIDI-instrument tracks coexist in playback and export.
- Preserve the existing signing/validation pipeline and describe MIDI/instrument-originated output
  truthfully without inventing unsupported C2PA assertions.

Audio recording, external MIDI hardware, tempo maps, CLAP, MPE, automation, warping, time
stretching, advanced routing, and full-DAW expansion remain outside PR 018.
