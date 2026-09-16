# WavMark Soft-Binding Recovery

This is a **local/offline proof of concept**, not a production C2PA Soft Binding Resolution
Service. A 16-bit WavMark payload is only unique within this machine's local store; it is not a
global identifier and does not establish ownership, rights, or identity.

```text
Render ──> WavMark payload ──> C2PA sign ──┬─> embedded C2PA manifest
                                           └─> exact .c2pa bytes in local store

Manifestless derivative ──> WavMark decode ──> alg + payload ──> local resolver
                                                       └───────> recovered signed manifest
```

## Runtime and audio adapter

Run `scripts/setup_wavmark.sh` once. It installs pinned Python dependencies, WavMark commit
`6ab3bf7ce0679e5b5cfeff3a62e8df9cd2024b37`, and the checksum-verified official model under:

```text
~/Library/Application Support/C2PA Creative Sequencer/WavMark/
```

The app invokes that helper only during export or explicit recovery. The final deliverable is
not converted to mono or 16 kHz: the helper derives a mono 16 kHz working signal, computes the
WavMark residual, resamples the residual to the render rate, applies it coherently to stereo,
writes 24-bit WAV, then downmixes/resamples the written result and requires an exact decode.

## Manifest and local resolver

Enabled export records `c2pa.watermarked.bound` and a `c2pa.soft-binding` assertion with
algorithm `com.microsoft.wavmark.1`, a whole-audio millisecond timespan, and the payload as the
pinned SDK's base64 JSON representation. `Builder::sign()`'s returned bytes are written without
reconstruction to:

```text
~/Library/Application Support/C2PA Creative Sequencer/SoftBindingStore/
  index.json
  manifests/<sha256>.c2pa
```

Recovery first confirms the asset has no embedded manifest, decodes WavMark, resolves the local
entry, inspects the stored manifest against the derivative, and independently checks that the
manifest contains the same algorithm and payload. Missing payloads, decode failures, corrupted
stores, and assertion mismatches are failures; there is no fallback provenance claim.

## Semantic limit

An embedded manifest is validated normally and always takes priority. A recovered manifest is
reported as `RECOVERED_SOFT_BINDING` and **not** as cryptographic validation of the derivative:
removing or rewriting the original container breaks the original hard binding even when the
audio watermark survives. Recovered state is transient UI information in this PR and is not
automatically propagated into a later export.
