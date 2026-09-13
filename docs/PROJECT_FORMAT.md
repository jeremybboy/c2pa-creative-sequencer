# Project Format

PR 003 introduces schema version 1 of the non-destructive project bundle. PR 004 begins populating its media, track, and clip records. A project is one directory whose name ends in `.c2paseq`:

```text
MyProject.c2paseq/
├── project.json
├── arrangement.tracktionedit
├── provenance.json
└── Media/
```

`project.json` is the application-owned creative model. It stores the project UUID, name, ISO 8601 creation and modification timestamps, BPM, application version, tracks, clips, media references, and reserved plug-in state records. Clip timing, gain, and fades are metadata: they never modify source bytes.

Each media reference contains a UUID, original filename, project-relative path, byte size, and lowercase SHA-256. `MediaLibrary` copies sources through a temporary file, verifies the copied SHA-256 before committing it, and reuses a previously registered asset with the same hash. PR 004 validates WAV, AIFF, and MP3 files by opening a decoder before copying them, then adds one non-destructive clip at the current playhead position; the source file is never modified.

`arrangement.tracktionedit` is Tracktion Engine's native XML edit state and is read or written only through `TracktionAdapter`. `provenance.json` is an application-owned companion document keyed to the project UUID; schema version 1 reserves ingredient-manifest IDs and action IDs without claiming C2PA parsing or signing.

Waveform data is derived from the copied project media by JUCE's `AudioThumbnail` using Tracktion Engine's thumbnail cache. It is rebuildable display data and is deliberately excluded from the portable project bundle.

## Versioning and validation

Both JSON documents declare `schemaVersion: 1`. Loading rejects unknown schema versions, missing companion files, mismatched provenance project IDs, out-of-range BPM, malformed arrays and scalar types, and media paths outside `Media/`; no schema migration is implemented yet.
