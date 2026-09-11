# C2PA Creative Sequencer

A minimal music sequencer for arranging, processing and remixing audio stems while preserving and exporting verifiable C2PA provenance.

![Overview of stems moving through arrangement and VST processing into a signed, verified WAV](docs/assets/repository-overview.svg)

The creative path remains primary: imported stems become a non-destructive arrangement, optional track processing contributes to a stereo mix, and a separate provenance layer describes meaningful ingredients and operations before C2PA signing and verification.

## Status

The repository is in version 0.1 proof-of-concept development. Dependency verification is complete; PR 001 establishes the native macOS application shell and Tracktion Engine boundary. Audio import, editing, VST3 hosting, rendering, and C2PA integration remain planned work and must not be described as implemented yet.

## Product boundary

The product is an arrangement-based audio-stem sequencer, not a full DAW. It will support multiple audio tracks, non-destructive clip editing, constrained VST3 effect hosting, stereo WAV export, and C2PA ingest/export; it will not support recording, MIDI, warping, automation lanes, complex routing, cloud storage, or collaboration.

## Architecture

JUCE owns the native application and UI, while Tracktion Engine owns audio devices, the timeline model, playback, plug-in processing, rendering, and undo. C2PA code will remain behind a dedicated application service so provenance serialization cannot leak into the audio engine.

See [dependency verification](docs/DEPENDENCIES.md) and [architecture notes](docs/ARCHITECTURE.md).

## Build

The supported POC target is Apple-silicon macOS 13.3 or newer with Xcode 16 or newer and CMake 3.27 or newer. Exact build commands are added and verified in PR 001.

## Known limitations

No creative audio workflow is implemented in the bootstrap. Commercial distribution requires licensing decisions for both JUCE and Tracktion Engine; this repository is not legal advice.
