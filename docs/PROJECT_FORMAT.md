# Project Format

PR 003 introduces schema version 1 of the non-destructive project bundle. PR 004 begins populating its media, track, and clip records, and PR 005 makes those records the canonical editable arrangement. A project is one directory whose name ends in `.c2paseq`:

```text
MyProject.c2paseq/
├── project.json
├── arrangement.tracktionedit
├── provenance.json
└── Media/
```

`project.json` is the application-owned creative model. It stores the project UUID, name, ISO 8601 creation and modification timestamps, BPM, timeline zoom/scroll, application version, tracks, clips, media references, and reserved plug-in state records. Track records persist order, name, gain, pan, mute, and solo. Clip records persist track membership, timeline start, source offset, and duration; these are non-destructive metadata and never modify source bytes.

Each media reference contains a UUID, original filename, project-relative path, byte size, and lowercase SHA-256. `MediaLibrary` copies sources through a temporary file, verifies the copied SHA-256 before committing it, and reuses a previously registered asset with the same hash. WAV, AIFF, and MP3 files are decoded before copying; dropping a file then adds one non-destructive clip to the chosen existing track at the chosen snapped time, creating additional empty tracks only when the target lane requires one. The source file is never modified.

`arrangement.tracktionedit` is Tracktion Engine's native XML edit state and is read or written only through `TracktionAdapter`. `provenance.json` is an application-owned companion document keyed to the project UUID; schema version 1 reserves ingredient-manifest IDs and action IDs without claiming C2PA parsing or signing.

Waveform data is derived from the copied project media by JUCE's `AudioThumbnail` using Tracktion Engine's thumbnail cache. It is rebuildable display data and is deliberately excluded from the portable project bundle.

Added Places roots are stored separately in the user's application-data directory as
`C2PA Creative Sequencer/places.json`. They are local browser preferences, are not part
of the project, and can be removed from the browser without affecting disk contents.

## Versioning and validation

Both JSON documents declare `schemaVersion: 1`. Loading rejects unknown schema versions, missing companion files, mismatched provenance project IDs, out-of-range BPM, malformed arrays and scalar types, and media paths outside `Media/`; no schema migration is implemented yet.
