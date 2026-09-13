# Architecture

The system keeps four concerns separate: creative state, audio execution, provenance state, and C2PA serialization. JUCE owns the application shell and custom interface; a narrow adapter shields application code from Tracktion Engine; the provenance service will translate a stable internal provenance model into `c2pa-cpp` calls only at its boundary.

The initial dependency and component flow is shown in the repository overview diagram. This document will expand only when implementation makes an architectural claim real.

## PR 001 boundary

`Application` controls lifecycle and destroys the window before the audio layer. `AudioEngine` is the application-facing boundary; `TracktionAdapter` is the only PR 001 class that includes Tracktion Engine headers or constructs `tracktion::engine::Engine`. `ArrangementView` renders an intentionally empty native JUCE surface and receives only a human-readable engine status string.

No project, transport, clip, plug-in, render, or provenance behavior exists in this slice.

## PR 002 boundary

`TracktionAdapter` owns one in-memory empty Tracktion `Edit` and exposes only application-level device and transport snapshots. The engine opens output channels only and disables system MIDI scanning; the app requests 48 kHz and 512 samples when the selected device reports those values as supported, otherwise it keeps that device's valid settings and displays the effective values.

The transport bar implements play/pause, stop-to-zero, loop enablement, a ten-minute seek range, position display, and BPM control with a 120 BPM default. The Audio Device button opens JUCE's output-only device selector; recording and input configuration remain outside the product boundary.

The `transport_foundation` test uses Tracktion's hosted-audio interface at 48 kHz/512 samples. It processes an empty Edit, proves every output sample remains zero, proves the underlying playhead advances by the processed duration, and proves stop plus seek positions are deterministic without relying on physical CI audio hardware.

## PR 002 verification

- The app and both test executables build locally with Apple Clang 21.
- CTest passes `application_skeleton` and `transport_foundation` (2/2).
- On the selected MacBook Pro Speakers device, the live UI reported the effective 48 kHz sample rate and 512-sample block size.
- Accessibility-driven runtime checks proved Play changed to Pause, position advanced from `00:00.000` to `00:01.024`, Pause held `00:00.757` unchanged, Stop returned it to `00:00.000`, Loop toggled on, and the Audio Device dialog opened.
- Repeated startup checks exposed and then eliminated a block caused by Tracktion opening persisted SoundFlow MIDI endpoints; system MIDI enumeration is disabled because MIDI is outside this product's scope.
- A standard application quit event terminated the process.

## PR 003 boundary

`ProjectEngine` coordinates lifecycle without leaking Tracktion types into the project model. `Project`, `ProjectSerializer`, `ProjectPaths`, and `MediaLibrary` use JUCE core types only; `TracktionAdapter` alone creates, saves, and loads `arrangement.tracktionedit`.

The application exposes New, Open, and Save controls for `.c2paseq` bundles. A successful new-project operation creates all four required bundle entries, save synchronises the effective Tracktion BPM into `project.json`, and open validates both JSON documents before replacing the active Tracktion Edit.

Project JSON, Tracktion edit XML, and provenance JSON remain separate because they have different owners and evolution paths. The project test proves schema round-trip and byte-identical media copying with SHA-256; it does not claim playable audio import, waveform generation, or C2PA validation.

## PR 003 verification

- The app and all three test executables build locally with Apple Clang 21.
- CTest passes `application_skeleton`, `transport_foundation`, and `project_model` (3/3).
- A live macOS run created a project through the native save panel and produced `project.json`, `arrangement.tracktionedit`, `provenance.json`, and `Media/`.
- The two JSON documents parsed successfully, and the Tracktion file contained native Edit XML.
- After saving and quitting, a fresh app process opened the same bundle and restored its project identity.
