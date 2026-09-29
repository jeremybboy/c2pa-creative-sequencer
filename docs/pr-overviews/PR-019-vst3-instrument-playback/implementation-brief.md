# PR 019 — One VST3 Instrument per MIDI Track

- **Base:** `main` after merged PR 018
- **Branch:** `pr/019-vst3-instrument-playback`
- **Pull request:** [#19](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/19)
- **Merge rule:** human acceptance required; do not merge automatically

## Goal

Turn PR 018's editable MIDI notes into audible and exportable arrangement material without adding
live keyboard input, MIDI recording, plug-in chains, or general-purpose DAW routing.

## Implemented

- MIDI tracks list scanned VST3 instruments; Audio tracks continue to list effects only.
- Each MIDI track owns exactly one instrument slot. Selecting another instrument replaces it.
- Beat-based MIDI clips and notes are mirrored into Tracktion and scheduled through the instrument.
- MIDI tracks use the existing mute, solo, gain, pan, level-meter, save/reopen, and missing-plug-in behavior.
- Offline rendering includes instrument output, including MIDI-only arrangements, before the existing C2PA signing and validation pipeline.
- A deterministic test synth verifies discovery, role rejection, replacement, persistence, scheduling, and non-silent rendering without requiring Surge XT in CI.

## Explicitly deferred

- Laptop-keyboard and external MIDI input.
- Monitoring of live notes, record arm, and MIDI recording.
- Multiple instruments, instrument/effect chains, advanced routing, automation, and plug-in sandboxing.
- New plug-in-specific C2PA assertions; the export remains a digital creation and does not invent instrument authorship claims.

## Verification

- Fresh Release configure and application build succeeded.
- Focused `plugin_hosting` test passed with the deterministic VST3 effect and synth fixtures.
- Full normal CTest suite passed 16/16 in 49.92 seconds after all targets were built.
- Surge XT manual acceptance remains required before merge.

## Manual acceptance

Create a MIDI track and clip, draw notes, scan and load Surge XT, verify arranged playback and the
track mixer, save/reopen and confirm the instrument state, replace the instrument in the same slot,
then export and confirm the WAV contains the instrument-rendered audio and valid Content Credentials.
