# Architecture

The system keeps four concerns separate: creative state, audio execution, provenance state, and C2PA serialization. JUCE owns the application shell and custom interface; a narrow adapter shields application code from Tracktion Engine; the provenance service translates the stable internal provenance model into `c2pa-cpp` calls only at its boundary.

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

## PR 004 boundary

`ProjectEngine` validates and registers imported WAV, AIFF, and MP3 files, while
`TracktionAdapter` creates one full-length audio clip on a new track. The
application treats imported stems as native-speed, absolute-time audio: embedded
loop tempo and root-note metadata are retained in the source file but must not
silently enable Tracktion auto-tempo, auto-pitch, looping, or time stretching.

`NativeAudioClipPolicy` applies that rule after insertion. Loading an early PR 004
project also removes automatic stretch state and restores the full source duration,
so projects created before the fix remain playable without re-importing media.

## PR 004 playback regression

Tracktion automatically interpreted an ACID-tagged 125 BPM chord loop as an
auto-tempo/auto-pitch clip, changed its duration from 7.68 seconds to 8.00 seconds
at the project's 120 BPM, and produced a zero-valued hosted output because this
POC intentionally has no time-stretch backend enabled. The regression test now
creates a synthetic loop-tagged WAV that triggers the same state, applies the
native playback policy, checks that the original duration is restored, and proves
that the hosted output has a non-zero peak.

## PR 005 boundary

PR 005 keeps seconds as the canonical saved and playback coordinate. `TimelineGeometry`
is the only conversion layer between seconds and pixels; it also derives beat/bar
spacing from BPM, performs beat snapping, and preserves the time beneath the zoom
anchor. The ruler, grid, clips, playhead, seek gestures, and scroll range all consume
that same geometry, so no independent fixed-card or decorative timeline coordinate
exists.

`Project` remains the application-owned creative source of truth. Every clip command
updates its track assignment, start, source offset, or duration in that model;
`ProjectEngine` then rebuilds the Tracktion Edit, reapplies `NativeAudioClipPolicy`,
saves both representations, and records the prior project snapshot for undo. A failed
rebuild or save restores the prior model and Edit. Tracktion owns audio execution,
not the user-facing arrangement identity.

Clip order within a saved track is also playback priority. During an Edit rebuild,
`ClipOcclusion` subtracts every later clip's time range from earlier clips and emits
only the remaining playback segments, with corrected source offsets. The canonical
clips and source media are unchanged, so moving or deleting a priority clip restores
the underlying audio; occlusion never crosses track boundaries.

Mute and Solo are live track-state changes rather than arrangement rebuilds. Their
model values and Tracktion track values are updated through one command, then saved
without replacing the active Edit, which preserves both transport state and playhead.
The keyboard mapper routes Space, save, undo/redo, duplicate, split, and delete to the
same `ArrangementView` command methods used by visible controls where those exist.

`PlacesStore` persists multiple absolute sample-folder roots in the user's application
data directory. `PlacesBrowser` reads folders lazily, displays only directories and
supported audio files, and emits file references for drag placement; it never writes
to or deletes from the source tree. Places are intentionally machine-local and do not
enter the portable project bundle.

## PR 005 verification

- Timeline tests prove seconds/pixel round trips, beat/bar duration, snap behavior,
  zoom-anchor stability, and Places add/deduplicate/reload/remove persistence.
- Arrangement-rule tests prove later clips occlude earlier clips only on the same
  track, deleting the priority clip restores the original range, different tracks
  retain simultaneous ranges, and all required keyboard mappings are present.
- Project-model tests prove track state, clip placement/source offset/duration, and
  timeline zoom/scroll survive JSON save and load.
- The production-engine integration test proves import to an existing track, horizontal
  move, vertical reassignment, two-edge trim state, duplicate, split, delete, track
  controls, live Mute/Solo without transport stop or reset, undo/redo, and exact
  save/reopen restoration.
- The existing hosted playback regression still proves loop-tagged audio retains native
  duration/pitch and generates non-zero output.
- The remaining mouse-feel, visual, and listening workflow is explicitly manual and must
  be completed before merge; a successful build does not substitute for that acceptance.

## PR 006 boundary

`RenderService` derives an export plan from canonical project state. It excludes muted
tracks and non-soloed tracks when any solo is active, applies the same non-destructive
same-track occlusion policy used by realtime playback, ends at the last audible segment,
and deduplicates contributing media by project media identity. `TracktionAdapter` performs
the resulting offline stereo 24-bit WAV render; no C2PA code participates in audio mixing.

`ProvenanceService` is the only production class that includes `c2pa.hpp`. Import inspects
the byte-identical project media copy and stores an application-owned status summary in
`project.json`: `VALID`, `PRESENT_WITH_VALIDATION_ISSUE`, `NO_CREDENTIALS`, or
`UNABLE_TO_VALIDATE`. Absence of credentials is a neutral state, not an integrity failure.

`ExportController` owns the deterministic pipeline: render to an uncommitted temporary
WAV, optionally build one new final claim, add each actual source once as `componentOf`,
sign and embed, reopen and validate, then atomically commit. A signing or validation
failure leaves no destination that could be mistaken for authenticated output. If test
signing is not configured, the ordinary WAV is committed only with an explicit unsigned
result.

The signing provider reads `C2PASEQ_SIGNING_BUNDLE_PEM` only at operation time. The
provided SEC1 EC key is converted to the PKCS#8 representation required by the SDK in
memory; the key, converted bytes, and certificate are never stored in project data,
logs, Git, or the application bundle.
