# PR 022 — Stem provenance authoring foundation

## Scope and starting point

Base: merged PR 021 on `main` (`924127d`). Branch: `pr/022-stem-provenance-foundation`.
The sequencer already renders MIDI/VST3 into final mixes, signs completed WAV exports, and
attaches contributing imported media as ingredients. It does not create separately signed
MIDI stems or record audio. This PR adds only reusable internal stem authoring capability.
The [editable overview](overview.svg) and [rendered overview](overview.png) separate this
slice from later user workflows.

## Change and provenance ledger

| Area | Before | PR 022 | Explicitly untouched |
| --- | --- | --- | --- |
| Action construction | Fixed final-mix action history | Explicit action definitions with optional source type, description, agent, parameters | Default created/digitalCreation and watermark-bound action conditions |
| Signing | Completed mix via `signWav` | Shared signing core plus standalone `signStemWav` | Signer ownership, SDK, integrity policy, ingredient and binding serialization |
| Stem descriptions | No reusable templates | MIDI created/rendered and capture created/digitalCapture templates | No plugin/device/performer/demo values invented |
| Publication | Export soft-binding outbox | Stem method has no publication or binding inputs | ExportController and resolver remain unchanged |
| Failure outputs | Caller could retain stale validation | Reset validation at entry; manifest-store bytes exposed only after validation | No unsigned fallback or weakened validation |
| User workflows | Final mix export | No new controls | Bounce, capture, imported-audio fallback, recovery, DSP |

The brief comes from the user-provided three-PR plan; repository state was checked locally
before implementation. The diagram is a scope map, not a screenshot or a claim that future
UI exists. No assets, performers, instruments, or rights are inferred from a file.

## Architecture and contracts

- `ProvenanceService` remains the only signing/inspection owner; both paths build, sign,
  embed, reopen, and require C2PA presence plus asset integrity.
- `signWav` keeps its caller signature and default actions, ingredients, soft-binding data,
  and optional validated manifest-store output; ExportController is untouched.
- `signStemWav` accepts a titled `audio/wav` descriptor and explicit action list, with no
  ingredients, soft-binding assertions, watermark processing, fingerprint processing, or outbox.
- Templates identify the application, not the VST instrument or input device; future callers
  must provide truthful structured execution metadata. Creation source types belong to the
  specific template, not a universal classification of all audio.
- The SDK upgrades actions to v2, accepts generator-info agents, and validates required fields.
  Optional parameters are objects. Its inspection JSON can represent byte-like integer arrays
  as base64, so future schemas need concrete round-trip tests.
- The API is low-level: an error can leave a destination artifact, and later orchestrators
  must stage and import only a successful validated result.

## Verification

Verified locally on 2026-10-09; human code review/merge pending.
The existing `build-pr021` Release directory is reused with the PR 022 sources and pinned
dependencies; its name does not imply older binaries.

| Exact command | Result |
| --- | --- |
| `cmake --build build-pr021 --config Release --target C2PAProvenanceTests -j 4` | Focused target built after schema tests were corrected for SDK requirements |
| `ctest --test-dir build-pr021 -R '^provenance_service$' --output-on-failure` | 1/1 passed |
| `cmake --build build-pr021 --config Release -j 4` | Release app and all normal test targets built; upstream Tracktion warnings remain |
| `cmake --build build-pr021 --config Release --target C2PACreativeSequencer -j 4` | Explicit Release application target passed |
| `ctest --test-dir build-pr021 --output-on-failure` | 17/17 passed in 16.96 s with localhost permission; initial sandbox run passed 16/17 and failed only the resolver HTTP bind with `Operation not permitted` |
| `ctest --test-dir build-pr021 -R '^(provenance_service\|export_pipeline\|soft_binding\|project_model)$' --output-on-failure` | 4/4 passed in 11.95 s |
| `codesign --verify --deep --strict 'build-pr021/C2PACreativeSequencer_artefacts/Release/C2PA Creative Sequencer.app'` | Passed |
| `git diff --check` | Passed |

The SVG was rendered to PNG with the bundled Sharp library and visually checked for clipping
and accurate scope labels. Production changes were reviewed: no plugin names, MIDI values,
tempo, performer, device, or demo identifiers are hardcoded. User WAV files remain untracked.

Focused coverage inspects actual embedded manifests using the pinned SDK and upstream test
signer: two deterministic WAV fixtures, created/rendered and digitalCapture actions, descriptions,
software agents, nested parameter objects, absent optional fields, no stem ingredients/bindings,
readable signed WAVs, unchanged unsigned input, and clean failures for missing/malformed input,
non-object parameters, invalid embedded history, missing signer, and invalid signer configuration.
These fixtures test schema/signing only; they are not MIDI-render or human-capture evidence.

The normal suite also covers final export with no binding, watermark-only, fingerprint-only,
combined bindings, ingredient handling, publication packages, and project import. Optional real
AudioWMark/audfprint integration tests are not required for this non-DSP change and are not claimed.

## Planned separately / human review

- PR 023 (not implemented): render a real MIDI instrument track, gather project/plugin metadata,
  sign the stem, import on an audio track, and prevent double playback.
- PR 024 (not implemented): approved minimal audio capture with real device/take metadata,
  signed WAV import, and natural ingredient propagation.
- Review this API and evidence before merging; there is no new UI to exercise in PR 022.
  No automatic merge, recording, MIDI bounce, or fabricated final-mix ingredient is included.
