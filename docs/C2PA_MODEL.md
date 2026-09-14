# C2PA POC Model

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

Import copies the source bytes into the project media directory, then inspects that copy.
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

`C2PASEQ_SIGNING_BUNDLE_PEM` selects an external development credential. The supplied
C2PA Conformance credential is test material, not a production identity. It is ignored by
Git, never copied into the app or project, and never logged. Its SEC1 key is represented as
PKCS#8 in memory because that is the format accepted by the pinned SDK.

Signing, embedded-manifest validation, and external trust-list recognition are distinct.
The export is committed as authenticated only after it reopens with a manifest and its
protected asset data validates. If signing or final validation fails, the destination is
not committed. If signing is unconfigured, export produces an explicitly reported unsigned
WAV. Detailed edit history, VST provenance, AI classification, and continuously signed
project state are deferred.
