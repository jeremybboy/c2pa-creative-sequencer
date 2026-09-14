<img width="1441" height="870" alt="Screenshot 2026-09-13 at 4 33 55 PM" src="https://github.com/user-attachments/assets/128e6809-61e0-4e41-9ac3-42ee8514d26c" />
# C2PA Creative Sequencer

A minimal music sequencer for arranging, processing and remixing audio stems while preserving and exporting verifiable C2PA provenance.

![Overview of stems moving through arrangement and VST processing into a signed, verified WAV](docs/assets/repository-overview.svg)

The creative path remains primary: imported stems become a non-destructive arrangement, optional track processing contributes to a stereo mix, and a separate provenance layer describes meaningful ingredients and operations before C2PA signing and verification.

## Status

The repository is in version 0.1 proof-of-concept development. PRs 001–005 provide the native macOS shell, audio transport, project bundles, stem import/playback, and focused Arrangement editing. PR 006 adds offline stereo 24-bit WAV export, source Content Credentials inspection, exact contributing-ingredient selection, and optional signed C2PA WAV export followed by immediate validation. VST3 hosting and detailed edit provenance remain future work.

## Product boundary

The product is an arrangement-based audio-stem sequencer, not a full DAW. It will support multiple audio tracks, non-destructive clip editing, constrained VST3 effect hosting, stereo WAV export, and C2PA ingest/export; it will not support recording, MIDI, warping, automation lanes, complex routing, cloud storage, or collaboration.

## Architecture

JUCE owns the native application and UI. The application project model owns canonical clip timing in seconds and edit history, while Tracktion Engine executes and offline-renders the mirrored arrangement. All SDK access is isolated in `src/provenance`; UI and project code consume only application-owned provenance records and export results.

See [dependency verification](docs/DEPENDENCIES.md) and [architecture notes](docs/ARCHITECTURE.md).

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

Select a clip and click **Credentials** to inspect its import-time C2PA status. Click **Export** for a stereo 24-bit WAV ending at the last audible clip. Without `C2PASEQ_SIGNING_BUNDLE_PEM`, export deliberately produces a clearly reported unsigned WAV; set that variable to the external Conformance test bundle PEM to enable test-signed export.

## Known limitations

The POC still has no recording, MIDI, warping, time stretching, plug-in UI, automation, advanced routing, or detailed edit provenance. The supplied Conformance certificate proves test signing and asset integrity but is not a production identity; external trust recognition depends on the verifier's trust configuration. Added Places are machine-local, imported media is copied byte-for-byte into `Media/`, and editing remains non-destructive. On one track, a later clip has priority in overlaps; different tracks mix normally, and export uses that same arrangement.
