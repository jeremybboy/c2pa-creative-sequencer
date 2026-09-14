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
