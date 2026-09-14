<img width="1439" height="871" alt="Screenshot 2026-09-13 at 7 23 22 PM" src="https://github.com/user-attachments/assets/96730edc-50c2-4a1c-a4ae-d49e0c3e8526" />

# C2PA Creative Sequencer

A minimal music sequencer for arranging, processing and remixing audio stems while preserving and exporting verifiable C2PA provenance.

**C2PA Creative Sequencer acts as a C2PA Claim Validator when media enters the creative workflow and a C2PA Claim Generator when the final mix is exported.**

![Overview of stems moving through arrangement and VST processing into a signed, verified WAV](docs/assets/repository-overview.svg)

The creative path remains primary: imported stems become a non-destructive arrangement, optional track processing contributes to a stereo mix, and a separate provenance layer describes meaningful ingredients and operations before C2PA signing and verification.

## Status

The repository is in version 0.1 proof-of-concept development. PRs 001–005 provide the native macOS shell, audio transport, project bundles, stem import/playback, and focused Arrangement editing. PR 006 makes source Content Credentials validation automatic on import and makes normal Export a mandatory render → claim → sign → embed → reopen → validate pipeline for stereo 24-bit WAV. PR 008 adds one constrained VST3 audio-effect slot per track; detailed plug-in provenance remains future work.

## Product boundary

The product is an arrangement-based audio-stem sequencer, not a full DAW. It will support multiple audio tracks, non-destructive clip editing, constrained VST3 effect hosting, stereo WAV export, and C2PA ingest/export; it will not support recording, MIDI, warping, automation lanes, complex routing, cloud storage, or collaboration.

## Architecture

JUCE owns the native application and UI. The application project model owns canonical clip timing in seconds and edit history, while Tracktion Engine executes and offline-renders the mirrored arrangement. All SDK access is isolated in `src/provenance`; UI and project code consume only application-owned provenance records and export results.

See [dependency verification](docs/DEPENDENCIES.md), [architecture notes](docs/ARCHITECTURE.md), and [VST3 hosting](docs/VST_HOSTING.md).

## Build

The supported POC target is Apple-silicon macOS 13.3 or newer with Xcode 16 or newer and CMake 3.27 or newer.

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open "build/C2PACreativeSequencer_artefacts/Debug/C2PA Creative Sequencer.app"
```

The first configure downloads the exact JUCE, Tracktion Engine, `c2pa-cpp`, and macOS arm64 C2PA runtime revisions recorded in [dependency verification](docs/DEPENDENCIES.md). The CI workflow uses the Xcode generator on GitHub's macOS runner.

## Arrangement controls

Add sample roots with **Places → Add Folder…**, expand their folders, and drag supported audio directly to a track and musical position. Clips snap to beats by default; hold **Option** while dragging to bypass snap. **Space** toggles play/pause at the current playhead, **Command-S** saves, and **Command-Z** / **Shift-Command-Z** undo and redo. Use **Delete**, **Command-D**, and **Command-E** for delete, duplicate, and split-at-playhead; use the **−/+** buttons or Command-scroll to zoom, Shift-scroll to move horizontally, and ordinary scroll to move vertically.

Imported files are inspected automatically and clips show **CC**, **No CC**, or **CC ?**; select a clip and click **Credentials** for the validation summary. The first **Export** asks for a PEM signing bundle, validates it with `c2pa-cpp`, stores a machine-local copy with restrictive permissions, and continues automatically. Later Finder launches reuse that credential; **Signing** shows configured state and provides replace/remove actions. Normal Export never silently falls back to unsigned WAV, while `C2PASEQ_SIGNING_BUNDLE_PEM` remains a developer-only override.

Each track header has a **+ VST** control. Choose **Scan VST3** explicitly to inspect the standard user and system VST3 folders, then choose one scanned audio effect; the same menu opens its editor, toggles bypass, or removes it. The scan cache is reused at startup, while projects retain plug-in identity, bypass state, and opaque parameter state without copying plug-in binaries. See [VST3 hosting](docs/VST_HOSTING.md) for the exact scope and acceptance procedure.

## Known limitations

The POC still has no recording, MIDI/instrument hosting, warping, time stretching, automation, plug-in chains, advanced routing, plug-in sandboxing, or detailed plug-in/edit provenance. The supplied C2PA Conformance credential is a **test credential only**, not the future production identity; external trust recognition depends on the verifier's trust configuration. The private PEM is stored outside projects and Git at `~/Library/Application Support/C2PA Creative Sequencer/Signing/signing-bundle.pem` with mode `0600` inside a `0700` directory. Added Places are machine-local, imported media is copied byte-for-byte into `Media/`, and editing remains non-destructive. On one track, a later clip has priority in overlaps; different tracks mix normally, and export uses that same arrangement.
