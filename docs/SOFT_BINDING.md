# Audio Soft-Binding Authoring and Recovery Demo

This is a local, SBR-inspired architecture experiment, not a production resolver and not a claim of C2PA SBR conformance. Watermark and fingerprint discovery share one exact manifest repository but are deliberately different:

- **Watermark:** something is added to the content. AudioWMark embeds a random 128-bit lookup identifier; recovery decodes it and performs exact repository membership lookup.
- **Fingerprint:** something is calculated from the content. audfprint derives acoustic landmarks; recovery performs a similarity search against registered references.

```text
AUTHORING
Sequencer -> render -> [optional AudioWMark embed] -> verify audio
          -> fingerprint final audio essence -> C2PA soft-binding assertion(s)
          -> sign/embed -> validate -> signed WAV + publication outbox

RECOVERY — WATERMARK
Derivative -> AudioWMark decoder -> exact identifier lookup -> manifest repository -> .c2pa

RECOVERY — FINGERPRINT
Derivative -> audfprint landmarks -> similarity index -> manifest repository -> .c2pa
```

## External runtimes

Run `scripts/setup_audiowmark.sh` and `scripts/setup_audfprint.sh`. The pinned runtimes live outside Git and the app bundle under Application Support. AudioWMark 0.6.5 (`c204998c92931285efdf6670c81cefd199298895`) is GPL-3.0-or-later. dpwe/audfprint (`cb03ba99feafd41b8874307f0f4e808a6ce34362`) is MIT-licensed and uses FFmpeg, NumPy, and SciPy. Neither implementation is linked into the Sequencer or used on its realtime audio thread.

The experimental identifiers are `io.github.jeremybboy.audiowmark.1` and `io.github.jeremybboy.audfprint.1`; neither is an official C2PA SBAL registration. C2PA 2.4 assigns one `alg` to a soft-binding assertion, so the app emits separate `c2pa.soft-binding` assertions for the two algorithms. Only a real watermark adds `c2pa.watermarked.bound`; there is no invented fingerprint action.

## Export and publication

With both controls enabled, the worker-thread sequence is:

```text
render stereo WAV -> AudioWMark embed -> verify stereo/rate/duration/24-bit
-> compute audfprint .afpt from watermarked PCM -> construct assertions
-> C2PA sign/embed -> reopen/validate -> publish -> atomic final WAV commit
```

Signing changes container metadata after the fingerprint is calculated; it does not alter PCM. With only fingerprint enabled, render goes directly to fingerprinting. With neither enabled, the pre-existing signed export behavior is unchanged.

Version-2 publication packages remain backward-compatible with PR 011:

```text
SoftBindingOutbox/<publication-id>/
  manifest.c2pa              exact bytes returned by c2pa-cpp
  binding.json               versioned list of bindings; legacy watermark fields retained
  fingerprint.json           audfprint profile, commit, hash count, registration value
  fingerprint-data.afpt      landmark hashes only
```

Packages contain no WAV, project media, private key, or PEM. The fingerprint value in the C2PA assertion is the SHA-256 identifier of the exact `.afpt` registration bytes; similarity comes from audfprint's aligned landmark search, not from comparing that identifier.

## Resolver and matching rule

Start `python3 tools/softbinding-resolver/server.py` and open `http://127.0.0.1:8787`. `/watermark` uses exact identifier lookup; `/fingerprint` uses a separate audfprint database. Both import the same outbox idempotently and return exact bytes from `repository/manifests/`.

The fingerprint acceptance rule is fixed at **at least 10 time-aligned landmark hashes** using audfprint's exact-count matcher. The UI also reports raw common hashes, query hash count, query coverage, offset, and matched time support. The threshold is stricter than audfprint's default 5 and was selected before the MP3 result; it was not lowered to make the test pass. A match can be false-positive evidence and a non-match can be a false negative; neither is cryptographic verification.

Measured opt-in result on 2026-09-18: a signed, watermarked, fingerprinted 12-second WAV was transcoded with libmp3lame at 192 kbps. The MP3 matched with **229 aligned hashes**, 265 raw common hashes, 659 query hashes, 34.75% query coverage, and 10.52 seconds of support. Exact manifest bytes were recovered. Deterministic unrelated noise returned zero matches. On that same MP3, AudioWMark decoded eight candidates but none matched the registered identifier; its failure is reported rather than hidden or retuned.

Recovery wording is intentionally limited: **Content Credentials recovered via audio fingerprint** or **Content Credentials recovered via audio watermark**. The derivative has not passed the original asset's cryptographic hard binding.
