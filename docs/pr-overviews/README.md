# Pull request overview archive

This directory pairs each substantial implementation brief with a one-glance visual summary.
The artifacts explain the verified starting point, requested change, explicit non-goals,
architecture constraints, verification evidence, and remaining human acceptance work.

GitHub is the authority for current pull-request and merge status. Status shown here is a dated
snapshot retained to explain what was known when the overview was prepared.

| PR | Overview | Status snapshot | Pull request |
| --- | --- | --- | --- |
| 022 | [Stem provenance authoring foundation](PR-022-stem-provenance-foundation/implementation-brief.md) · [Visual](PR-022-stem-provenance-foundation/overview.png) · [Editable SVG](PR-022-stem-provenance-foundation/overview.svg) | Release build, 17/17 tests, focused checks, and signature pass on 2026-10-09; review pending; no bounce or recording UI | See GitHub for current status |
| 021 | [Recovery demo launcher](PR-021-recovery-demo-launcher/implementation-brief.md) · [Editable visual](PR-021-recovery-demo-launcher/overview.svg) | Implementation, 17-test suite, signature, and local UI smoke pass; independent human acceptance pending | Not opened |
| 020 | [Explicit computer-keyboard monitoring](PR-020-computer-keyboard-monitoring/implementation-brief.md) · [Visual](PR-020-computer-keyboard-monitoring/overview.png) | Implementation and automated verification complete; human acceptance passed on 2026-09-29 | [#20](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/20) |
| 019 | [VST3 instrument playback + render](PR-019-vst3-instrument-playback/implementation-brief.md) | Merged after manual acceptance on 2026-09-29 | [#19](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/19) |
| 018 | [MIDI foundation + piano roll](PR-018-midi-foundation/implementation-brief.md) · [Visual](PR-018-midi-foundation/overview.png) · [Editable SVG](PR-018-midi-foundation/overview.svg) | Merged: MIDI model, arrangement editing, and piano roll; no instrument playback | [#18](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/18) |
| 017 | [Arrangement Selection, Clipboard, Looping + Live Mixer Controls](PR-017-arrangement-editing/implementation-brief.md) · [Visual](PR-017-arrangement-editing/overview.png) | Local and GitHub automated acceptance passed; human acceptance pending on 2026-09-28 | [#17](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/17) |

## Archive convention

Each PR folder should contain:

- `implementation-brief.md`: goal, change ledger, behavior, boundaries, test evidence, and manual acceptance;
- `overview.svg`: editable visual source when practical;
- `overview.png`: a synchronized rendered visual that adds no behavior absent from the text brief.

Use `PR-NNN-short-name/` for folder names. Update both artifacts when scope changes; do not leave
a stale graphic beside a corrected brief. Do not store application builds, test fixtures, source
media, credentials, or generated exports in this archive.
