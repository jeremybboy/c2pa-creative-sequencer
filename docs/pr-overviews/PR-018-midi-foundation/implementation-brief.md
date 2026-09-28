# PR 018 — MIDI v1: Piano Roll, Instruments, Live Input, Recording + Render

![PR 018 checkpoint overview](overview.png)

- **Repository:** `jeremybboy/c2pa-creative-sequencer`
- **Base:** `main` after PR 017
- **Branch:** `pr/018-midi-foundation`
- **Pull request:** [#18](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/18)
- **Current checkpoint:** foundation and piano-roll editing complete; VST3 instrument hosting is next
- **Merge rule:** remain open and unmerged until all checkpoints and human acceptance pass

## Goal

Deliver one basic, usable MIDI v1 inside PR 018 through sequential checkpoint commits: create and
edit MIDI clips, play them through a VST3 instrument, play and record from the laptop keyboard,
render instrument output into the existing audio mix, and pass that mix through the existing C2PA
export pipeline. Planned checkpoints are targets, not claims about current behavior.

## Transparency / change ledger

| Area | Current PR 018 status |
| --- | --- |
| Current state | Foundation and piano-roll checkpoints are complete: beat-based MIDI clips/notes are visible, editable, undoable, and persistent, but intentionally silent. |
| Target user-visible change | Complete a basic MIDI workflow through separately verified piano-roll, instrument, live-input, recording, and render/C2PA checkpoints. |
| Code/data layers to touch | Project model, arrangement and piano-roll UI, Tracktion MIDI scheduling, VST3 instrument hosting, keyboard input, recording, offline render, tests, and documentation. |
| Explicitly untouched | Audio recording, warping, time stretching, automation, complex routing, cloud services, MPE, CLAP, and full-DAW expansion. |
| Persistence/undo impact | Foundation persistence and migration are complete. Each later editing/recording gesture must have explicit persistence and undo tests. |
| Audio/realtime impact | None in the completed foundation or piano roll. The next instrument/input checkpoints must prove realtime safety and preserve existing Audio behavior. |
| C2PA/provenance impact | None in the completed foundation. The final checkpoint must reuse the existing export/sign/validate path and document MIDI/instrument output truthfully. |
| Verification evidence | Foundation evidence is recorded below. Piano roll: clean Release build, focused arrangement-editing test, and full 16/16 normal CTest passed; human UI acceptance remains pending. |
| Human acceptance | Required after every checkpoint and again for the complete end-to-end MIDI workflow before merge. |

## IMPLEMENTED

- Explicit `Audio` and `MIDI` track types with stable identity.
- Audio clips remain in seconds; MIDI clips and notes store positions and durations in beats.
- Central constant-BPM beat/second conversion boundary.
- MIDI clips/notes persist pitch, start, duration, velocity, and identity in project schema 2.
- Legacy schema-1 tracks without a type load as Audio.
- **+ Track** creates Audio or MIDI tracks; MIDI tracks support rename, delete, undo/redo, and
  save/reopen without presenting fake audio controls.
- Existing Audio tracks and clips coexist with the MIDI foundation.
- A MIDI-lane time selection exposes **Create Empty MIDI Clip** on double-click and creates a clip
  matching the selected range exactly.
- MIDI clips are visible and support arrangement select, move, trim, delete, copy, cut, paste,
  duplicate, loop, undo/redo, and save/reopen.
- Double-clicking a MIDI clip opens a piano roll with pitch keys, an absolute song-bar ruler,
  clip boundaries, velocity lane, and pointer-dependent pinch zoom for time or pitch range.
- Notes support draw, select/multi-select, live-feedback pitch/time move, resize, velocity edit, delete,
  **Command-A/C/X/V/D**, beat snapping, and one undo entry per committed edit.
- The piano roll shows the arrangement playhead, aligned to absolute musical time, while transport
  crosses the open MIDI clip.
- MIDI data remains beat-based across BPM changes. There is intentionally no MIDI sound yet.

## NOT YET IMPLEMENTED

1. **Current checkpoint — VST3 instrument:** instrument selection/state, MIDI scheduling, and
   realtime playback.
2. **Planned — live keyboard:** laptop-key mapping, monitoring, octave control, all-notes-off safety.
3. **Planned — recording:** record arm, note capture during transport, editable recorded clips.
4. **Planned — render + C2PA:** offline instrument audio in the mix and truthful integration with
   the existing C2PA export pipeline.

## UNCHANGED

- Existing Audio clip editing, overlap priority, transport, mixer, metering, VST3 audio effects,
  sample audition, and project behavior.
- Existing signing credentials, C2PA ingest/export validation, AudioWMark, fingerprinting, and
  local resolver behavior until the final integration checkpoint explicitly exercises export.
- The product remains a minimal arrangement sequencer rather than a general-purpose DAW.

## VERIFICATION

Checkpoint 1 foundation evidence already recorded:

- clean Release configure/build succeeded;
- normal local CTest suite passed 16/16 in 33.31 seconds;
- GitHub macOS `build-and-test` completed successfully;
- local launch verified Audio/MIDI creation, a truthful MIDI row, rename, save, and mixed-project
  reopen;
- opt-in AudioWMark/audfprint runtime tests were not run because the foundation did not change
  soft-binding behavior.

Checkpoint 0 changes only documentation and the visual. Its fresh Release build succeeded and the
final full normal CTest run passed 16/16 in 14.49 seconds. An initial sandboxed run could not bind
the resolver's loopback port; on the first permission-corrected run, `export_pipeline` transiently
segfaulted during device initialization after passing in the prior run. The isolated export test
then passed, followed by the clean 16/16 full-suite result. No product code changed. No later
checkpoint may begin until this checkpoint is pushed, reported, and explicitly approved by the
human reviewer.

Checkpoint 2 piano-roll evidence:

- clean Release build succeeded;
- focused `C2PAArrangementEditingTests` passed, covering default and exact-range clip creation,
  MIDI clip/note editing, velocity, multi-note clipboard/delete, duplication with fresh identities,
  beat preservation across BPM changes, loop derivation, undo/redo, and save/reopen;
- final full normal CTest passed 16/16 in 16.92 seconds;
- manual UI acceptance remains pending before this checkpoint is approved;
- no MIDI audio execution, instrument hosting, recording, or C2PA behavior was added.

## Planned final acceptance

Create Audio and MIDI tracks, create/edit a MIDI clip in the piano roll, load a VST3 synth, hear
arranged and live laptop-keyboard notes, record and edit a performance, manipulate MIDI clips,
save/reopen, render Audio plus instrument output, and validate the result through the existing C2PA
export pipeline. None of this end-to-end result is claimed until it is actually verified.
