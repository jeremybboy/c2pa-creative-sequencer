# Development Rules

## Product boundary

This repository builds a minimal arrangement-based creative audio sequencer with native C2PA provenance. It is not a general-purpose DAW. The explicit build and exclusion lists in the approved product specification are binding.

## Change workflow

After the bootstrap commit, every implementation slice must use its own branch and the sequence: implementation, tests, local build, documentation, explicit-path staging, commit, and human-reviewed pull request. Never merge a pull request, push without explicit permission, force-push, use broad staging, or perform destructive repository operations.

## Evidence rules

Do not claim that a build, launch, audio result, plug-in behavior, credential, signature, or export works unless it was verified. Keep creative state, audio execution, provenance state, and C2PA serialization separate.

## Scope guard

If an implementation seems to require recording, MIDI, warping, automation, complex routing, cloud services, or another excluded subsystem, stop and redesign within scope rather than adding it.
