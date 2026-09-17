# Audio Soft-Binding Authoring and Recovery Demo

PR 011 separates the claim generator from the recovery consumer. It is a local architecture
demonstration, not a production C2PA Soft Binding Resolution Service and not a claim of C2PA SBR
API conformance.

```text
AUTHORING SIDE
Creative Sequencer -> render -> AudioWMark embed -> C2PA soft-binding assertion
                   -> C2PA sign/embed -----> signed WAV
                                      `----> publication outbox
                                             (binding + exact .c2pa)

RECOVERY SIDE
Manifestless derivative -> browser -> POST /matches/byContent -> local resolver
                                                               |- AudioWMark decode
                                                               `- exact binding lookup
                                         -> manifest repository -> recovered .c2pa
```

## Authoring side

Run `scripts/setup_audiowmark.sh` once. It builds AudioWMark 0.6.5 at commit
`c204998c92931285efdf6670c81cefd199298895` outside the repository and app bundle under:

```text
~/Library/Application Support/C2PA Creative Sequencer/AudioWMark/
```

AudioWMark is GPL-3.0-or-later. The Sequencer does not link it; `AudioWMarkService` invokes its
native executable as an external process. Enabled export generates 16 random bytes, represents
them as 32 lowercase hexadecimal characters, and performs exactly one watermark operation:
`audiowmark add`. The experimental identifier is
`io.github.jeremybboy.audiowmark.1`; it is **not** an official C2PA Soft Binding Algorithm List
registration.

The background export sequence is render -> watermark embed -> format verification -> create
the C2PA 2.4 `blocks` soft-binding assertion and `c2pa.watermarked.bound` action -> sign/embed ->
reopen/validate -> publish -> atomically commit. The final WAV remains stereo, 24-bit, at the
render sample rate and duration. No watermark decode occurs in production export and PCM is not
changed after signing.

After signing, the Sequencer writes the exact manifest-store bytes returned by `c2pa-cpp`:

```text
~/Library/Application Support/C2PA Creative Sequencer/SoftBindingOutbox/<binding-id>/
  manifest.c2pa
  binding.json
```

The package contains no key, PEM, audio, or project media. It is a publisher handoff, not the
resolver repository.

## Recovery side

The independent resolver owns:

```text
~/Library/Application Support/C2PA Soft Binding Demo/repository/
  index.json
  manifests/<sha256>.c2pa
```

Start it with `python3 tools/softbinding-resolver/server.py`, open
`http://127.0.0.1:8787`, import Sequencer publications, and drop a derivative into the browser.
The service runs `audiowmark get`, checks every 128-bit candidate against its repository, and
reports recovery only on exact membership. It implements `POST /manifests`, `POST /bindings`,
`POST /matches/byContent`, `POST /imports/sequencer`, `GET /matches/byBinding`,
`GET /manifests/{manifestId}`, and `GET /services/supportedAlgorithms`.

AudioWMark can emit false candidate patterns. The first candidate is never treated as
provenance; only a repository match counts. A recovered signed manifest remains evidence linked
through a soft binding: the derivative has not passed the original asset's cryptographic hard
binding. A future production publisher can replace the local outbox with remote
`POST /manifests` and `POST /bindings` without changing the Sequencer's provenance semantics.

See `docs/BENCHMARKS.md` for measured export timings and `docs/DEMO.md` for acceptance steps.
