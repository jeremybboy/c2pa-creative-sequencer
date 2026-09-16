# Architecture

The system keeps four concerns separate: creative state, audio execution, provenance state, and C2PA serialization. JUCE owns the application shell and custom interface; a narrow adapter shields application code from Tracktion Engine; the provenance service translates the stable internal provenance model into `c2pa-cpp` calls only at its boundary.

The initial dependency and component flow is shown in the repository overview diagram. This document will expand only when implementation makes an architectural claim real.

## PR 001 boundary

`Application` controls lifecycle and destroys the window before the audio layer. `AudioEngine` is the application-facing boundary; `TracktionAdapter` is the only PR 001 class that includes Tracktion Engine headers or constructs `tracktion::engine::Engine`. `ArrangementView` renders an intentionally empty native JUCE surface and receives only a human-readable engine status string.

No project, transport, clip, plug-in, render, or provenance behavior exists in this slice.

## PR 002 boundary

`TracktionAdapter` owns one in-memory empty Tracktion `Edit` and exposes only application-level device and transport snapshots. The engine opens output channels only and disables system MIDI scanning; the app requests 48 kHz and 512 samples when the selected device reports those values as supported, otherwise it keeps that device's valid settings and displays the effective values.

The transport bar implements play/pause, stop-to-zero, arrangement-aware loop enablement, a ten-minute seek range, position display, and BPM control with a 120 BPM default. Loop resolves to the selected clip or, with no selection, the full arrangement; its persisted range is mirrored into Tracktion and highlighted in the ruler. The Audio Device button opens JUCE's output-only device selector; recording and input configuration remain outside the product boundary.

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
the original source without modifying it and stores an application-owned status summary in
`project.json`: `VALID`, `PRESENT_WITH_VALIDATION_ISSUE`, `NO_CREDENTIALS`, or
`UNABLE_TO_VALIDATE`. Absence of credentials is a neutral state, not an integrity failure.

`ExportController` owns the deterministic pipeline: require a configured signer, render to an
uncommitted temporary WAV, build one new final claim, add each actual source once as `componentOf`,
sign and embed, reopen and validate, then atomically commit. A signing or validation
failure leaves no destination that could be mistaken for authenticated output. Missing
configuration is a hard failure in the normal Export path; there is no unsigned fallback.

The signing provider prefers a developer `C2PASEQ_SIGNING_BUNDLE_PEM` override when set;
otherwise it reads the one-time GUI-selected bundle from the app's private Application Support
directory. `ProvenanceService` validates and constructs the signer; the UI only selects a file
and reports state. A supplied SEC1 EC key is converted to the PKCS#8 representation required
by the SDK in memory; private material is never stored in project data, logs, Git, CI artifacts,
source media, or the application bundle.

## PR 008 boundary

`PluginScanner` performs only explicit VST3 scans of the standard macOS user and
system locations and persists JUCE's metadata cache outside projects. `PluginHost`
is the application-facing load/open/bypass/remove boundary; `PluginWindow` owns a
native editor when supplied and falls back to JUCE's generic parameter editor.
No application class talks directly to Steinberg VST3 interfaces.

`TracktionAdapter` notifies `PluginHost` before replacing an Edit. The host destroys
every editor window before Tracktion releases its corresponding processor, preventing
editor timers or callbacks from retaining deleted processor references. Pressing an
editor's close button also releases the owned editor instead of only hiding it.

The project model remains authoritative for one optional effect slot per track,
including identity, metadata, opaque state, bypass, and missing status. A project
rebuild asks `TracktionAdapter` to insert that external effect before Tracktion's
track volume/pan plug-in. A missing or failed plug-in never substitutes another
binary: the stored record survives, processing is bypassed, and the track remains
usable and removable.

Tracktion Engine owns the single processing graph used by hardware playback and
`Renderer`, so PR 008 adds no parallel DSP or export-only plug-in path. The export
controller remains render → claim → sign → embed → reopen → validate;
the rendered PCM now includes enabled track VST3 processing before provenance is
attached. Detailed plug-in/AI assertions are explicitly deferred to PR 009.

Instrument hosting is deferred because the sequencer has no MIDI model and adding
one solely to audition Surge XT would violate the product boundary. The inspected
AI reference repository is a standalone SwiftUI application with no VST3 build
target, so PR 008 does not claim integration until that external project supplies
an Apple-silicon effect bundle and its runtime/assets.

## PR 011 soft-binding boundary

PR 011 does not change embedded-manifest inspection. `ExportController` optionally
orchestrates render → payload allocation → WavMark embed → exact decode verification →
C2PA sign/embed → reopen validation → exact manifest-store persistence → atomic output
commit. A failure in any enabled stage is fatal; disabled export follows the pre-PR-011
path and creates no watermark or recovery record.

`WatermarkService` is the application boundary for non-realtime model work. The production
`WavMarkService` invokes a machine-local Python helper; no model loading, resampling, or
inference occurs in Tracktion's realtime graph. The adapter downmixes and resamples only a
working representation to mono 16 kHz, embeds there, resamples the residual to the original
rate, applies it coherently to both channels, writes stereo 24-bit WAV, and requires exact
decode from the final file.

`ProvenanceService` remains the only C2PA SDK boundary. It adds the C2PA 2.4 `blocks`
soft-binding structure and `c2pa.watermarked.bound`, captures the exact bytes returned by
`Builder::sign()`, and can inspect those bytes against a derivative. `SoftBindingStore`
maps `com.microsoft.wavmark.1` plus the 16-bit payload to those bytes; recovery additionally
requires the stored manifest's own algorithm and value to match, so an index hit is not proof.
Recovered results set `RECOVERED_SOFT_BINDING`, force `assetIntact = false`, and never claim
that the derivative satisfies the original manifest's cryptographic hard binding.
