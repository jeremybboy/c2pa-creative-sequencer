# PR 020 — Explicit computer-keyboard monitoring

## Goal

Let the user temporarily play the selected MIDI track's existing VST3 instrument from the Mac
keyboard. Listening starts only after the user turns on the toolbar keyboard control; track selection
alone never captures typing.

## Verified starting point

- PR 018 supplies MIDI tracks, clips, notes, arrangement editing, and piano-roll editing.
- PR 019 supplies one replaceable VST3 instrument per MIDI track, arranged-note playback, state
  persistence, mixer control, and offline rendering.
- No computer-keyboard monitoring or MIDI recording exists on merged `main`.

## Change ledger

| Area | Before | PR 020 target |
| --- | --- | --- |
| Activation | No live note input | Explicit toolbar keyboard toggle |
| Routing | Arranged notes only | Transient notes target the selected MIDI track's enabled instrument |
| Plug-in focus | Monitoring stops receiving keys outside the arrangement window | The explicit toggle remains active while the selected instrument editor has focus |
| Mapping | None | A–L note layout, including black-key row; Z/X octave |
| Safety | Not applicable | Key-up, focus-loss, target-change, export, and toggle-off note cleanup |
| Persistence | Arranged notes persist | Live monitoring remains transient and is never written to the project |
| Track creation | Rebuilds the Edit and stops playback | Audio or MIDI track insertion preserves active transport |
| Clip creation | Empty-space double-click | Right-click the selected range and choose Create Empty MIDI Clip |
| Trim preview | Waveforms and notes appear stretched during a drag | Crop/reveal content at a stable time scale during the drag |

## Architecture and constraints

- Convert keys to MIDI note events through a small mapping unit independent of the UI.
- Inject live events into the existing Tracktion AudioTrack that owns the selected MIDI instrument.
- Do not start, stop, or reposition transport when monitoring notes.
- Do not record or serialize transient note events.
- Do not route to Audio tracks, missing instruments, or bypassed instruments.
- Preserve normal text entry and Command/Control/Option shortcuts.

## Explicitly unchanged / out of scope

- MIDI recording, record arm, external MIDI hardware, and MIDI-file import.
- Audio recording, automation, warping, time stretching, MPE, routing expansion, and CLAP.
- Instrument hosting, arranged-note scheduling, export, C2PA, watermark, and fingerprint behavior.

## Verification plan

Automated checks must cover the deterministic key mapping and live-event routing without changing
transport state. The complete Release test suite and application build must still pass. Human
acceptance must verify explicit activation, chords, key release, Z/X octave changes, selected-track
routing, arrangement and plug-in-window focus, stopped/running transport behavior, typing/shortcut
guards, live track insertion, right-click clip creation, stable trim previews, and no stuck notes after
focus loss or deactivation.

## Acceptance state

- Release application and all test targets built successfully in `build-pr020`.
- The complete 16-test suite passed. The resolver test required localhost permission when rerun
  because the restricted test sandbox rejected its temporary loopback socket.
- Focused mapping and live-routing coverage passed as part of `arrangement_rules` and
  `plugin_hosting`.
- The arrangement regression verifies that clip editing and both Audio and MIDI track insertion
  leave transport playing and preserve the active playhead.
- The finished app bundle passes strict deep code-signature verification after ad-hoc re-signing;
  its existing build order adds the icon and recent-files nib after the earlier signing step.
- Human acceptance passed on 2026-09-29, including the playback-preservation follow-up.
