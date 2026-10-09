# PR 023 — Bounce MIDI track to credentialed stem

Base: merged PR 022 on `main`, `01faca9`; branch: `pr/023-midi-credentialed-stem`.
The work started as `codex/023-credentialed-midi-stem` and was renamed to the user's exact
branch without restarting or discarding edits. PR publication requires explicit push approval;
no merge is authorized. [Editable overview](overview.svg) · [Rendered overview](overview.png).

## Goal and transparency / change ledger

Create a real signed MIDI/VST3 WAV ingredient rather than describing an unrendered MIDI
object as if it were already a credentialed audio asset.

| Area | Current starting point | Change in this PR | Untouched |
| --- | --- | --- | --- |
| Signing | PR 022 standalone stem API | Call existing created/rendered signing and integrity inspection | Credential storage, SDK, trust policy, stem action templates |
| Audio execution | MIDI contributes to whole-mix renders | Explicit single-track bit, staged pre-mixer WAV, restored audibility | Existing mix render parameters and ExportController |
| Creative state | MIDI clips + one instrument | New audio track, signed media, source mute in one undo item | Original notes/instrument stay editable; no schema additions |
| Metadata | Live plugin/project state available | Deterministic MIDI and binary state hashes, real names/counts/BPM | No raw state, inferred rights, hardcoded demos, or recording claims |
| UI | Instrument menu and track headers | Bounce command and signed-stem Credentials details | Piano-roll behavior, MIDI import/recording, broad UI redesign |
| Final provenance | Imported media ingredients | Signed WAV naturally becomes a credentialed ingredient | No fake MIDI ingredients or altered final-export actions |
| Recovery | Existing AudioWMark + audfprint | No changes | Decoder parameters, thresholds, resolver, startup-test issue |

This brief follows the user-provided PR 023 guidance. The diagram is a scope/flow map, not
a screenshot or evidence of human listening acceptance. Local automated verification passed;
human instrument/listening acceptance remains pending. Recording is a separate PR 024 proposal.

## Architecture and behavior

- `RenderService` builds a metadata plan from the captured canonical track and instrument state.
  Content hashing excludes UUIDs and object memory; the exact JSON encoding is documented in
  [C2PA_MODEL](../../C2PA_MODEL.md).
- `TracktionAdapter` uses an explicit bit in `Renderer::Parameters::tracksToDo`. The pinned
  `toBitSet` helper enumerates all tracks even for a subset, so it is not used to select a stem.
  Mute/solo overrides and mixer bypass are exclusive, temporary, and restored on error/success.
- Stem audio contains the instrument output before track gain/pan and excludes master processing.
  The new audio track inherits source gain/pan so track pan law and master processing are not
  printed twice. A level-equivalence test compares signed final renders before/after bounce.
- Range covers the earliest non-empty clip start to the latest non-empty clip end. Output is
  imported at that start, preserving song alignment. Automatic release/reverb tails are not added.
- `ProjectEngine` prepares/validates, renders/signs, then validates and imports. Preparation and
  atomic import run on the owner thread; render/sign runs on an exclusive worker through the
  `AudioEngine` facade. The instrument editor closes after successful preflight.
- Only a readable, integrity-validated signed WAV matching the prepared source signature/range
  is copied by the normal `MediaLibrary` path. One undo item adds track/clip/media and mutes MIDI;
  solo transfers if necessary. Save/reopen uses the existing schema. Undo retains media bytes
  under the existing import/redo policy.
- UI is exclusive during bounce, with stage-boundary cancellation, temporary-file cleanup, and
  a normal-Quit guard while the renderer needs the message thread. Playback pauses and position
  is retained. Failed/cancelled bounces do not add a track or mute the source.
- `ProvenanceService` remains the only signer/inspector. The signed source is an ordinary media
  ingredient in final export; no final-export orchestration, bindings, or recovery paths change.

## Verification

Verified locally on 2026-10-09. The reused `build-pr021` directory contains current PR 023
sources, not an old PR 021 application. Commands and results:

```sh
cmake --build build-pr021 --config Release -j 4
cmake --build build-pr021 --config Release --target C2PACreativeSequencer -j 4
ctest --test-dir build-pr021 --output-on-failure
ctest --test-dir build-pr021 -R '^(plugin_hosting|provenance_service|export_pipeline|arrangement_editing)$' --output-on-failure
git diff --check
codesign --verify --deep --strict 'build-pr021/C2PACreativeSequencer_artefacts/Release/C2PA Creative Sequencer.app'
```

Both Release builds passed; full CTest passed **17/17** in **24.11 seconds**; focused CTest
passed **4/4** in **15.35 seconds**. Tests ran with localhost permission; existing recovery
startup/resolver tests were not modified. Diff whitespace and code-signature checks passed.
The editable SVG was rendered to PNG and visually inspected: readable panels, no clipping.
No live-app UI acceptance or human Surge XT listening result is claimed.

Coverage includes isolated render vs unrelated audio, restored mute/solo and position, readable
non-silent WAV, actual actions/metadata/hashes, unsigned rejection, aligned import, source mute,
solo transfer, one undo/redo, save/reopen, final-export nested manifest linkage, level preservation,
and failures for no project, wrong/empty/instrumentless target, bypass/missing plugin, stale source,
invalid render, and signing failure. A worker-render test dispatches the message loop as the UI does.
These are real test-synth renders, not proof that every external VST3/preset sounds identical.

## Human acceptance / stop conditions

Follow [the PR 023 steps in DEMO](../../DEMO.md#pr-023-credentialed-midi-stem-acceptance).
Check your installed Surge XT/preset, timing, gain/pan, undo/reopen, credentials, final ingredient,
and cancellation. No human listening or external-instrument acceptance is claimed yet.

Do not broaden this PR into recording, imported-audio recording fallback, MIDI-file import/export,
AI source taxonomy, plugin-specific schemas, multi-instrument routing, tempo stretching, recovery,
or a fix to the existing CI startup test. Do not push without permission or merge automatically.
