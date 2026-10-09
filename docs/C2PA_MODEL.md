# C2PA POC Model

**C2PA Creative Sequencer acts as a C2PA Claim Validator when media enters the creative workflow and a C2PA Claim Generator when the final mix is exported.**

The implemented model is intentionally small:

```text
C2PA-aware or ordinary source
  -> ingest / read / validate
  -> normal creative arrangement
  -> offline stereo WAV render
  -> one new final claim
  -> Conformance test signing
  -> embedded Content Credentials
  -> immediate reopen / verification
```

## Source state

Import inspects the original source automatically, associates the result with its source-media
record, and copies the source bytes into the project media directory without modifying the original.
The project stores a display-oriented status and manifest summary, never a private key.
`NO_CREDENTIALS` means only that no C2PA manifest was found; it is not invalid or
suspicious. A manifest can be present while validation reports an issue, and a protected
asset can remain intact even when the signer is not recognized by an external trust list.

## Final claim

Export creates one claim describing a new WAV created by C2PA Creative Sequencer and its
application version. It uses `c2pa.created` with the standard digital-creation source type.
Each unique source media item that contributes audible audio is added once with the
`componentOf` relationship. If a source already has C2PA data, the SDK reads it while
forming the ingredient; an ordinary source remains an unsigned ingredient and is never
misrepresented as authenticated.

Contribution is computed from the final arrangement, not the media bin. Muted tracks,
tracks excluded by Solo, deleted clips, media no longer used by a clip, and Places entries
are excluded. Repeated clips using the same project media identity map to one ingredient.

## Signing and failure behavior

The first normal Export prompts for a PEM when no signer is configured, validates its
certificate and key by constructing a `c2pa-cpp` signer, and stores a private machine-local
copy in the app's Application Support directory. The directory is mode `0700` and the PEM
is mode `0600`; neither path nor private material enters Git, the app bundle, project data,
source media, CI artifacts, or logs. **The supplied C2PA Conformance credential is a test
credential only and is not the future production identity.** `C2PASEQ_SIGNING_BUNDLE_PEM`
remains available only as a developer override. A supplied SEC1 key is represented as PKCS#8
in memory because that is the format accepted by the pinned SDK.

Signing, embedded-manifest validation, and external trust-list recognition are distinct.
The export is committed as authenticated only after it reopens with a manifest and its
protected asset data validates. If signing or final validation fails, the destination is
not committed. Missing or invalid credentials, claim construction failure, signing/embedding
failure, or post-export validation failure produces an explicit error and no successful output;
normal Export has no unsigned fallback. Detailed edit history, VST provenance, AI
classification, and continuously signed project state are deferred.

## Standalone stem authoring foundation (PR 022)

Final mix export still renders and signs the completed mix through `signWav`, including
its existing ingredients, optional soft bindings, and publication path. PR 022 adds
`signStemWav` as an internal API, not a new export workflow or UI control. It takes a
`StemProvenanceDescriptor` containing a WAV title, `audio/wav` format, and explicit actions,
uses the configured signer, embeds the manifest, and reopens the result for integrity
validation. It attaches no ingredients or soft bindings and does not publish anything.

`makeMidiRenderedStemActions` describes `c2pa.created` / `digitalCreation`, followed by
`c2pa.rendered`; `makeHumanRecordedStemActions` describes `c2pa.created` / `digitalCapture`.
Both name the application as software agent and accept optional caller-supplied parameter
objects. They do not discover plugin, MIDI, performer, or device state and are not evidence
that rendering or capture occurred. Tests use deterministic synthetic WAVs to exercise real
SDK signing and inspect the action schemas, not to claim a human performance or MIDI render.

The pinned c2pa-cpp / c2pa-rs SDK accepts these fields and reports the embedded assertion as
`c2pa.actions.v2`; the agent is represented as a generator-info object with `name`.
Absent optional fields are omitted. An invalid history (for example, `c2pa.created` without
the source type required by this SDK) is not accepted as a successful signed stem.
Structured parameters are objects; nested objects, scalar values, and string arrays are
covered by tests. SDK inspection can normalize byte-like integer arrays to base64 strings;
later callers must verify their concrete metadata schema rather than assume byte-for-byte
JSON round trips. The action structure follows the
[C2PA actions specification](https://spec.c2pa.org/specifications/specifications/2.4/specs/C2PA_Specification.html).

Missing inputs, invalid descriptors/credentials, signing errors, or failed embedded validation
return failure. Validation output is reset at entry so a previous success cannot survive an
early error. Like the underlying SDK call, this low-level API may leave a destination file
on failure; future orchestrators must stage output and import it only after success, as the
existing final ExportController does. Credential trust recognition remains separate from
asset integrity, with the existing inspection policy unchanged.

PR 022 itself added no MIDI bounce or recording UI. Its schema fixtures do not constitute
rendering or capture evidence. Actual recording remains planned separately as PR 024,
subject to its separately approved recording scope.

## MIDI/VST3 credentialed stems (PR 023)

With a signer configured, right-click a MIDI track header or open its instrument menu and
choose **Bounce to Credentialed Audio Stem**. The track must have notes and a loaded,
non-bypassed VST3 instrument. Live instrument state is captured before hashing and the
instrument editor closes during the exclusive offline operation. Rendering/signing runs on
a worker; canonical import is committed on the UI/owner thread only after success.

Tracktion's renderer filters to the selected track via an explicit track bit, not a whole-mix
render. Temporary mute/solo overrides are never persisted and are restored on failure and
success. The render excludes master processing and the track's mixer: the resulting audio
track inherits source gain/pan, avoiding a second application of the engine's pan law.
Playback pauses, but its position is retained. Cancellation discards the result before import;
it waits for the current render/sign stage rather than interrupting plugin DSP or signing.
Normal quitting is blocked while a bounce worker needs the message thread.

The range begins at the earliest non-empty MIDI clip start and ends at the last non-empty
clip end; the new clip is placed at that same arrangement start. No automatic instrument-tail
allowance or new tempo-stretch behavior is added. Extend the MIDI clip before bouncing if its
release/reverb needs more space. Existing audio, other instruments, loop selection, and soft
bindings are not included in the standalone stem.

The real unsigned WAV is signed through PR 022's `signStemWav`, with `c2pa.created` /
`digitalCreation` followed by `c2pa.rendered`. The render parameters under
`c2paseq:midiRender` record the source track name, BPM, clip/note counts, MIDI-content SHA-256,
instrument name/vendor/version/format/identifier, and captured instrument-state SHA-256.
They also record the render range and that gain/pan/master processing were not printed.
The MIDI hash uses JUCE compact JSON in stored clip/note order, with clip start/length and
note pitch/start/duration/velocity; IDs and object addresses are excluded. Instrument state
is hashed as decoded binary, never embedded raw in the manifest. These are application
parameters, not a plugin-specific standardized provenance schema or an authorship claim.

After embedded integrity inspection, `MediaLibrary` copies the signed source unchanged into
normal project media. A new **<MIDI track name> Rendered Stem** audio track, its clip/media
reference, and source mute are one undoable mutation. If the original was soloed, solo moves
to the stem. Original MIDI notes and instrument remain editable. Save/reopen retains the
stem's credentials; undo removes project references but keeps media bytes, consistent with
existing imports and redo. Failures do not add a track or mute the source; staged files are
cleaned by the UI workspace.

Final `ExportController` behavior is unchanged: the audible signed WAV becomes a real
credentialed ingredient, and the SDK retains its manifest history. Direct, unbounced MIDI
is still rendered into the mix but is not fabricated into a separately signed ingredient.
The selected clip's **Credentials** dialog displays the actual stored actions and MIDI /
instrument metadata. Test signing credentials still do not imply external trust or conformance.
