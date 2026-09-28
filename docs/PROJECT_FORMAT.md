# Project Format

PR 003 introduced schema version 1 of the non-destructive project bundle. PR 018 introduces schema version 2, adding explicit Audio/MIDI track types and beat-based MIDI clip/note records while retaining seconds-based audio clips. A project is one directory whose name ends in `.c2paseq`:

```text
MyProject.c2paseq/
├── project.json
├── arrangement.tracktionedit
├── provenance.json
└── Media/
```

`project.json` is the application-owned creative model. It stores the project UUID, name, ISO 8601 creation and modification timestamps, BPM, timeline zoom/scroll, application version, tracks, clips, media references, and per-track VST3 state records. Track records persist stable UUID, explicit `audio`/`midi` type, order, name, gain, pan, mute, and solo. Audio clip records preserve seconds-based track membership, timeline start, source offset, and duration; these are non-destructive metadata and never modify source bytes. MIDI clips instead preserve start/length in beats plus stable note IDs, MIDI note number, clip-local start/duration in beats, and velocity. Beats are the single saved MIDI timing truth; execution seconds are derived through the project tempo.

Projects begin with four reusable Audio tracks but are not fixed to four. Add Track creates
either an Audio or MIDI track with a new stable UUID and type-specific sequential name. Deleting
one removes that track's project data and owned VST3 state without deleting source media; the
last remaining Audio track cannot be removed. Track creation/deletion participates in undo/redo
and is saved through the same ordered `tracks` array, so later track indices are derived from
persisted order rather than used as durable identity. PR 018 MIDI tracks intentionally have no
instrument, scheduling, audio output, or active mixer controls.

A VST3 state record is keyed to its owning track and persists the plug-in identifier, display metadata, format/category, bundle reference, JUCE identifiers, effect/instrument classification, bypass/missing flags, and the plug-in's opaque Base64 state. The binary is never copied into the project. If it cannot be restored, the project and track remain usable, the original identity and state are retained, and the slot is displayed as missing and bypassed until removed or the plug-in becomes available again.

Each media reference contains a UUID, original filename, project-relative path, byte size, and lowercase SHA-256. `MediaLibrary` copies sources through a temporary file, verifies the copied SHA-256 before committing it, and reuses a previously registered asset with the same hash. WAV, AIFF, and MP3 files are decoded before copying; dropping a file then adds one non-destructive clip to the chosen existing track at the chosen snapped time, creating additional empty tracks only when the target lane requires one. The source file is never modified.

`arrangement.tracktionedit` is Tracktion Engine's native XML edit state and is read or written only through `TracktionAdapter`. `provenance.json` is an application-owned companion document keyed to the project UUID; schema version 1 reserves ingredient-manifest IDs and action IDs without claiming C2PA parsing or signing.

Waveform data is derived from the copied project media by JUCE's `AudioThumbnail` using Tracktion Engine's thumbnail cache. It is rebuildable display data and is deliberately excluded from the portable project bundle.

Added Places roots are stored separately in the user's application-data directory as
`C2PA Creative Sequencer/places.json`. They are local browser preferences, are not part
of the project, and can be removed from the browser without affecting disk contents.

## Versioning and validation

`project.json` now declares `schemaVersion: 2`; `provenance.json` remains schema version 1 because PR 018 changes no provenance semantics. Loading accepts legacy project schema 1 and treats every track with no `type` as Audio, while every subsequent save emits schema 2. Unknown versions, missing companion files, mismatched provenance project IDs, out-of-range BPM, malformed values, invalid MIDI ranges/timing, cross-type clip contents, and media paths outside `Media/` are rejected.
