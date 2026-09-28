# MIDI v1 dependency sequence

PR 018 establishes only the product boundary, beat-based data model, serialization migration,
and explicit Audio/MIDI track creation. The remaining MIDI v1 should proceed in this order so
each layer can be tested without pretending that later capabilities already exist.

## PR 019 — MIDI Clip + Piano Roll Editing

- Create and open MIDI clips; show piano keyboard and note grid.
- Draw, select, move, resize, delete, and velocity-edit notes.
- Copy, paste, and duplicate notes.
- Move, trim, and duplicate MIDI clips in the arrangement.

## PR 020 — VST3 Instrument Hosting + MIDI Playback

- Allow VST3 instruments on MIDI tracks and schedule project MIDI notes into Tracktion.
- Use one deterministic test synth for realtime audible playback.
- Use the same instrument path for offline rendering and persist instrument state.

## PR 021 — Live Keyboard + MIDI Monitoring

- Map laptop keyboard input to note on/off with octave controls.
- Route selected or armed MIDI tracks while transport runs.
- Add stuck-note and all-notes-off safety.

## PR 022 — MIDI Recording

- Add record arm and capture incoming note events while transport runs.
- Reconstruct note duration and commit one beat-based MIDI clip as one undoable action.
- Support immediate playback and save/reopen.

## PR 023 — MIDI-to-Audio Export + Provenance

- Verify realtime and offline instrument output agree.
- Render MIDI instruments into the normal audio mix and preserve the C2PA export pipeline.
- Define truthful provenance semantics without inventing unsupported C2PA assertions.

CLAP hosting remains a separate later investigation. A CLAP fixture may inform that work, but it
must not block MIDI v1.
