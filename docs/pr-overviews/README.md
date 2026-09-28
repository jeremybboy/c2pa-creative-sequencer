# Pull request overview archive

This directory pairs each substantial implementation brief with a one-glance visual summary.
The artifacts explain the verified starting point, requested change, explicit non-goals,
architecture constraints, verification evidence, and remaining human acceptance work.

GitHub is the authority for current pull-request and merge status. Status shown here is a dated
snapshot retained to explain what was known when the overview was prepared.

| PR | Overview | Status snapshot | Pull request |
| --- | --- | --- | --- |
| 018 | [MIDI v1 — sequential implementation](PR-018-midi-foundation/implementation-brief.md) · [Visual](PR-018-midi-foundation/overview.png) · [Editable SVG](PR-018-midi-foundation/overview.svg) | Foundation complete; piano roll is the next checkpoint; later MIDI capabilities are planned, not implemented | [#18](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/18) |
| 017 | [Arrangement Selection, Clipboard, Looping + Live Mixer Controls](PR-017-arrangement-editing/implementation-brief.md) · [Visual](PR-017-arrangement-editing/overview.png) | Local and GitHub automated acceptance passed; human acceptance pending on 2026-09-28 | [#17](https://github.com/jeremybboy/c2pa-creative-sequencer/pull/17) |

## Archive convention

Each PR folder should contain:

- `implementation-brief.md`: goal, change ledger, behavior, boundaries, test evidence, and manual acceptance;
- `overview.svg`: editable visual source when practical;
- `overview.png`: a synchronized rendered visual that adds no behavior absent from the text brief.

Use `PR-NNN-short-name/` for folder names. Update both artifacts when scope changes; do not leave
a stale graphic beside a corrected brief. Do not store application builds, test fixtures, source
media, credentials, or generated exports in this archive.
