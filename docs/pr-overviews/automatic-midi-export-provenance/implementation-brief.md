# Follow-up — Automatic MIDI provenance during signed export

Repository: jeremybboy/c2pa-creative-sequencer.
Base: verified merged PR 023, `dbeb617`; branch: `codex/automatic-midi-export-provenance`.
No GitHub PR number is assigned yet. Do not merge or push without explicit approval.
[Editable overview](overview.svg) · [Rendered overview](overview.png).

## Goal

Let users draw MIDI, choose a VST3 instrument, and press signed Export normally, without a
manual bounce, extra checkbox, new visible track, or source mute. Generate real signed WAV
stems and use those actual audio assets in the final mix; retain manual bounce separately.

## Transparency / change ledger

| Area | Verified starting point | Change | Untouched |
| --- | --- | --- | --- |
| Workflow | PR 023 manual signed bounce | Automatic for signed Export | Manual bounce and project editing |
| Evidence | Real MIDI/instrument stem metadata | Capture live state into an export-only copy | No raw state, author/rights inference, or demo metadata |
| Audio | Direct MIDI in final mix | Independently render signed stems, replay them in export graph | Original track mixer/master processing, source files |
| C2PA | Explicit bounced WAV ingredients | Actual temporary signed inputs with nested history | Existing signer, action templates, embedding/validation policy |
| Persistence | Bounce adds media/track and undo | Automatic export adds no canonical state or undo | Serialization/schema; reusable manual stems |
| UI | Export progress and ingredient list | MIDI progress, count, and actual instrument details | No new checkbox or general DAW redesign |
| Recovery | Watermark/fingerprint final-export stages | No semantic change | Decoder settings, 10-hash threshold, resolver, startup test |

## Implementation discipline

1. Plan only MIDI allowed by current mute/solo policy, with notes and an active instrument.
2. Collect live state on the owner thread into a copy, not a project save or mutation.
3. Independently render each instrument pre-mixer, sign with PR 022/023 actions, and validate.
4. Stage signed files in the export graph at their original clip start, suppressing original
   MIDI/instrument execution temporarily; preserve original gain/pan and master processing once.
5. Let final signing consume those actual files, then restore the graph and delete temporary media.
6. On cancellation/failure restore graph/audibility/position; pre-commit failures preserve an
   existing destination under the existing temporary-file export path.

The code/data layers are ProjectEngine snapshot collection, RenderService planning,
ExportController orchestration, TracktionAdapter temporary routing, and UI progress/credentials.
ProjectEngine owns canonical creative state; only the playback edit is temporarily substituted.
No final ingredient is synthesized without a real signed WAV used in the mix.

An owner-thread reallocation inhibitor detaches playback for the worker scope, including
audio-only exports, and prevents edit timer/graph races. Original MIDI clip mutes and instrument
enable state are restored before
deleting private temporary files. Normal quitting waits for export/bounce completion.
Playback pauses but position is retained; cancellation is stage-boundary, not a DSP interrupt.

## Verification

Verified locally on 2026-10-09; external-instrument UI/listening acceptance is still required.
The reused `build-pr021` directory contains this follow-up's current sources, not the old PR 021 app.

```sh
cmake --build build-pr021 --config Release -j4
ctest --test-dir build-pr021 --output-on-failure
ctest --test-dir build-pr021 -R '^(export_pipeline|plugin_hosting)$' --repeat until-fail:2 --output-on-failure
git diff --check
codesign --verify --deep --strict 'build-pr021/C2PACreativeSequencer_artefacts/Release/C2PA Creative Sequencer.app'
```

Release build passed; full CTest passed **17/17** in **33.53 seconds**. Diff whitespace and
code-signature checks passed. Native tests need macOS audio/services and temporary-file access;
a sandbox run failed with `Operation not permitted`, not an audio/provenance result.
An initial full-suite worker-export crash exposed concurrent edit-timer playback rebuilding;
the shared owner-thread guard covers both audio-only and MIDI export. Recovery startup tests
and historical evidence are unchanged. Both export regression tests passed twice consecutively
in **48.03 seconds** after that fix. The editable SVG was rendered to PNG and visually checked:
readable panels, no clipping, starting/current/intended states explicitly separated.

Initial strict sample comparison showed a small phase difference from native WAV interpolation,
not a level change. The direct-MIDI comparison therefore checks exact
duration/sample rate, quarter-second RMS in each stereo channel within 0.1% amplitude (about
0.009 dB, with a 1e-6 absolute floor for silence), and end timing within 1 ms. Diagnostic WAVs
showed a two-sample interpolation delay and at most 1.84e-5 absolute window RMS difference;
fixed-window energy changes slightly with phase. Restoration of the original MIDI graph
retains a strict 1e-5 max-sample comparison; no production timing or gain was tuned to this fixture.
This does not claim bit-identical MIDI-to-WAV playback or arbitrary external-plugin equivalence.

Real test-VST3 + C2PA acceptance covers two MIDI tracks alongside imported audio, captured live
parameters, actual created/rendered histories nested in the final export, discarded intermediate
files, unchanged project files/media/tracks, original graph/transport restoration, solo/mute/bypass
eligibility, cancellation with an existing destination, stem/final signing failures, and manual-bounce
regression. No threshold or signing/validation policy is lowered to obtain a result.

## Human acceptance

Use [DEMO instructions](../../DEMO.md#automatic-midi-provenance-export-acceptance): play MIDI
through your installed synth, change parameters, Export without bounce, inspect credentials,
compare timing/level/pan, check no new tracks/mutes, then try two tracks and solo/mute controls.
External Surge XT listening/UI acceptance remains required; a build is not that evidence.

## Limits / stop conditions

No recording, MIDI-file import/export, stretch/warp, advanced routing, persistent stem cache,
tail detection, cloud SDK/service, key-management redesign, recovery changes, or automatic merge.
Every export renders fresh stems and takes longer for more MIDI tracks. Existing clip bounds
still limit instrument tails. Temporary asset paths are not reusable after export, but the final
manifest retains ingredient history; manual bounce is the route to a retained signed WAV.
Keep historical PR 023 evidence intact. Report missing runtime/source or external-plugin issues
honestly, and stop before any scope expansion or push without permission.
