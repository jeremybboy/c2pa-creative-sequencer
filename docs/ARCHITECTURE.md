# Architecture

The system keeps four concerns separate: creative state, audio execution, provenance state, and C2PA serialization. JUCE owns the application shell and custom interface; a narrow adapter shields application code from Tracktion Engine; the provenance service translates the stable internal provenance model into `c2pa-cpp` calls only at its boundary.

The initial dependency and component flow is shown in the repository overview diagram. This document will expand only when implementation makes an architectural claim real.

## PR 025 audio capture boundary

`TracktionAdapter` enumerates device-reported input channels and opens exactly one only on explicit
audio-track arm. `InputRecordingService` adds a JUCE device callback: raw input enters a bounded
`AudioFormatWriter::ThreadedWriter` queue; atomic peak/frame/failure snapshots feed the UI. The callback
performs no signing, file IO, project mutation, or audio-through. Its own output contribution is silence;
the existing arrangement callback continues to supply output. MIDI keyboard monitoring is separate.

The temporary workspace is unique per take. Stop detaches the callback writer, flushes off the audio
thread, checks mono/24-bit/rate/frame count, then signs the stable WAV with the existing human-recorded
`digitalCapture` action template. Only after embedded integrity validation does `ProjectEngine` make a
verified media copy into a candidate project, capture live plugin states, and commit one undoable clip
at the original recording position. Cancel/capture/signing failures clean temporary files without
unsigned import. Editing and project switching are blocked during the UI recording/finalization flow.

Arming and metering are transient machine-local execution state, never provenance or undo entries.
Input selection is session-local, not a portable hardware configuration. Metadata records the actual
input descriptor and accepted frames, not performer identity, room, device serial, or location.
There is no latency compensation, monitoring, punch, loop recording, comping, MIDI recording, cache,
or cloud subsystem. Existing automatic MIDI export and explicit credentialed bounce remain unchanged.

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
- Repeated startup checks exposed and then eliminated a block caused by Tracktion opening persisted SoundFlow MIDI endpoints. System MIDI enumeration remains disabled through PR 018 because live MIDI input is a later independently reviewed phase.
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
to or deletes from the source tree. `SampleAuditionPlayer` decodes the explicitly
selected file through JUCE and mixes one non-looping preview into the existing output
device without creating a clip or changing project transport. Replacing, stopping,
dragging, or importing clears the active preview. Places are intentionally machine-local
and do not enter the portable project bundle.

Audio-track creation and deletion are project-model mutations rather than UI-only lanes.
Adding a track assigns a stable UUID and the next default `Audio N` name; deleting a track
also removes any VST3 state owned by that UUID, while leaving source media untouched. Each
mutation rebuilds the mirrored Tracktion Edit, persists through `project.json`, and enters
the same undo/redo history as clip edits. The UI confirms deletion whenever a track owns
clips or a plug-in and prevents removal of the final audio track.

## PR 016 realtime meter and visual boundary

Each Tracktion audio track ends with an application-owned pass-through meter plug-in after
the hosted effect and volume/pan stages. Its realtime callback only reads the current audio
buffer and updates lock-free atomic peaks; it allocates nothing, takes no locks, performs no
I/O, and never changes samples. The message thread consumes those peaks at 30 Hz and applies
display-only scaling and release ballistics. Stopped, muted, and excluded-by-solo tracks decay
to silence, while offline rendering deliberately bypasses meter capture so export remains
unchanged.

`SequencerLookAndFeel` and `IconButton` provide the reusable visual layer for compact controls,
grouped toolbar hierarchy, and vector transport/history/zoom/audio/close icons. This layer owns
presentation only: existing Arrangement commands remain the single behavior path for mouse and
keyboard actions, and clip, browser, plug-in, project, export, and provenance semantics are not
changed.

## PR 017 arrangement editing and live mixer boundary

`ArrangementTimeSelection` is the explicit normalized selection model shared by the timeline
surface and clip views. Waveform-body and empty-lane drags create time selections; the compact
clip header remains the move handle and clip edges remain trim handles. Option bypasses beat
snapping for these gestures. Clipboard entries retain media identity, source offset, duration,
relative track, and relative time, so partial copy/cut/paste and duplicate remain non-destructive.
Each command mutates canonical project state once and therefore creates one undo snapshot.

Keyboard shortcuts call the same Arrangement command paths as visible controls. Paste resolves
its destination from the active time selection, explicit insertion point, or playhead in that
order. Loop Selection writes the exact selection or selected-clip bounds to the existing
Tracktion transport loop; ordinary Loop still uses the full arrangement when nothing is selected.
Timeline zoom remains a geometry-only operation, with Command-scroll and JUCE's native magnify
gesture preserving the time under the pointer.

Gain and pan drags are intentionally not arrangement rebuilds. A gesture snapshots project state
once, streams clamped values directly to the active Tracktion volume plug-in, then persists the
final values as one undoable edit when the gesture ends. This keeps the processing graph,
transport state, and playhead alive while the audible mix changes; save failure restores both the
project values and the live plug-in values.

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

Instrument hosting was deferred from PR 008 because that milestone had no MIDI
model or note-source path. PR 018 added the model boundary. PR 019 mirrors each
beat-based MIDI clip into Tracktion, schedules its notes into one VST3 instrument
slot on the owning MIDI track, and uses the same graph for hardware playback and
offline rendering. Track type is enforced at the boundary: Audio tracks accept one
effect, MIDI tracks accept one instrument, and selecting another plug-in replaces
the existing slot.

## PR 011 soft-binding boundary

PR 011 does not change import inspection: the Sequencer reads embedded C2PA only and a
manifestless derivative remains `NO_CREDENTIALS`. The optional authoring pipeline runs on a
worker thread and is render → random 128-bit allocation → one AudioWMark embed → audio-format
verification → C2PA sign/embed → reopen validation → outbox publication → atomic commit.
There is no production-path decode and no PCM mutation after signing.

`WatermarkService` remains the application boundary. `AudioWMarkService` invokes pinned
AudioWMark 0.6.5 as an external executable outside the bundle; GPL source is not linked into
the application or realtime graph. `ProvenanceService` remains the sole C2PA SDK boundary and
adds the C2PA 2.4 `blocks` soft-binding assertion plus `c2pa.watermarked.bound` using the single
experimental identifier `io.github.jeremybboy.audiowmark.1`, which is not an official SBAL
registration.

`SoftBindingOutbox` publishes `binding.json` and the exact manifest-store bytes returned by
`Builder::sign()`; it never writes audio, project media, PEM, or private keys. This publisher
handoff is separate from `tools/softbinding-resolver`, which owns decoding, repository lookup,
manifest retrieval, and the browser UI. The resolver imports the outbox idempotently, checks
every decoded candidate by exact 128-bit repository membership, and never treats soft-binding
recovery as proof that a derivative passes the original cryptographic hard binding.

## PR 012 dual soft-binding boundary

`SoftBindingClaim` is algorithm-neutral: it carries algorithm, type, bytes, and optional scope. C2PA 2.4 gives one algorithm to each soft-binding assertion, so watermark and fingerprint are separate `c2pa.soft-binding` assertions. The actions assertion adds `c2pa.watermarked.bound` only when the watermark service actually ran; fingerprinting adds no invented action.

`AudfprintService` is another worker-only external-process boundary. Export order is render → optional AudioWMark → format verification → audfprint registration from the final PCM essence → C2PA sign/embed → validation → versioned outbox publication → atomic commit. The app never queries a fingerprint database and exposes no recovery UI.

The same resolver imports both registration types into one repository. `bindings` remains the exact watermark index; `fingerprints` maps audfprint `.afpt` registrations to manifest IDs, and the resolver materializes audfprint's natural database for similarity queries. Both paths converge only after discovery at the byte-exact manifest store.

## PR 018 MIDI foundation boundary

The application-owned project model now distinguishes Audio and MIDI tracks explicitly. Audio
clips remain absolute-time source references in seconds. MIDI clips and their note events instead
store musical position and duration in beats; `MusicalTimeConverter` is the single constant-tempo
conversion boundary used to derive execution seconds. Changing BPM therefore changes derived time
without rewriting saved MIDI positions, and a future tempo map can replace the converter without
changing note semantics.

Project schema 2 stores track type and MIDI clip/note records. Schema 1 remains readable: a track
with no type is migrated in memory as Audio, and a later save writes schema 2. Provenance remains
schema 1 because this PR changes no provenance meaning. Audio and MIDI data cannot be mixed inside
the wrong track type, and existing audio media, plug-in, playback, export, and C2PA paths remain
unchanged.

The Add Track menu creates either type. MIDI rows persist identity, order, name, clips, notes, and
one optional VST3 instrument state. PR 019 activates their gain/pan, mute/solo, meter, arranged-note
playback, and offline rendering through Tracktion. Laptop-keyboard monitoring and MIDI recording
remain unavailable. See [MIDI v1 dependency sequence](MIDI_ROADMAP.md) for the follow-up phases.
