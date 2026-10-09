# Development Rules

## Product boundary

This repository builds a minimal arrangement-based creative sequencer with native C2PA provenance. It supports audio arrangements and an explicitly staged MIDI v1; it is not a general-purpose DAW. The explicit build and exclusion lists in the approved product specification are binding.

## Change workflow

After the bootstrap commit, every implementation slice must use its own branch and the sequence: implementation, tests, local build, documentation, explicit-path staging, commit, and human-reviewed pull request. Never merge a pull request, push without explicit permission, force-push, use broad staging, or perform destructive repository operations.

## Evidence rules

Do not claim that a build, launch, audio result, plug-in behavior, credential, signature, or export works unless it was verified. Keep creative state, audio execution, provenance state, and C2PA serialization separate.

## Scope guard

Approved MIDI v1 work may add MIDI tracks/clips/notes, beat-based note timing, piano-roll editing, VST3 instruments, computer-keyboard or external MIDI input, monitoring, record arm, MIDI note recording during transport, instrument playback/rendering, and truthful provenance for MIDI/instrument-originated output. PR 018 is the umbrella pull request for this MIDI v1, but every capability must arrive as a separate checkpoint commit: build, test, document, push, report, and wait for explicit human approval before the next checkpoint. The written implementation brief is authoritative; its visual must mark completed, current, and planned work without presenting future behavior as implemented. PR 025 explicitly authorizes minimal mono external audio capture on one armed Audio track, finalized and signed with digitalCapture semantics before verified media import. It does not authorize input monitoring/audio-through, loop recording, punch, comping, latency compensation, MIDI recording, cloud work, or persistent stem caches. If another implementation requires warping, time stretching, automation lanes, complex routing, cloud services, or an excluded subsystem, stop and redesign within scope rather than adding it.
