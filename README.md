# C2PA Creative Sequencer

A minimal music sequencer for arranging, processing and remixing audio stems while preserving and exporting verifiable C2PA provenance.

![Overview of stems moving through arrangement and VST processing into a signed, verified WAV](docs/assets/repository-overview.svg)

The creative path remains primary: imported stems become a non-destructive arrangement, optional track processing contributes to a stereo mix, and a separate provenance layer describes meaningful ingredients and operations before C2PA signing and verification.

## Status

The repository is in version 0.1 proof-of-concept development. PR 001 provides the native macOS application shell, PR 002 adds audio-device configuration and transport, PR 003 adds versioned `.c2paseq` project save/load, and PR 004 adds WAV/AIFF/MP3 stem import, waveforms, and playback. Clip editing, VST3 hosting, rendering, and C2PA integration remain planned work and must not be described as implemented yet.

## Product boundary

The product is an arrangement-based audio-stem sequencer, not a full DAW. It will support multiple audio tracks, non-destructive clip editing, constrained VST3 effect hosting, stereo WAV export, and C2PA ingest/export; it will not support recording, MIDI, warping, automation lanes, complex routing, cloud storage, or collaboration.

## Architecture

JUCE owns the native application and UI, while Tracktion Engine owns audio devices, the timeline model, playback, plug-in processing, rendering, and undo. C2PA code will remain behind a dedicated application service so provenance serialization cannot leak into the audio engine.

See [dependency verification](docs/DEPENDENCIES.md) and [architecture notes](docs/ARCHITECTURE.md).

## Build

The supported POC target is Apple-silicon macOS 13.3 or newer with Xcode 16 or newer and CMake 3.27 or newer.

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open "build/C2PACreativeSequencer_artefacts/Debug/C2PA Creative Sequencer.app"
```

The first configure downloads the exact JUCE and Tracktion Engine revisions recorded in [dependency verification](docs/DEPENDENCIES.md). The CI workflow uses the Xcode generator on GitHub's macOS runner.

## Known limitations

Each imported stem creates one track at the current playhead position and is copied byte-for-byte into the project before use. The current timeline is fixed at two minutes, does not scroll, and does not yet support moving, trimming, fading, deleting, or mixing clips; those are later slices.
