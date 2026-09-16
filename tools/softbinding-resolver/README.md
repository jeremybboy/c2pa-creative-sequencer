# C2PA SBR-Inspired Local Demonstration Service

This independent local tool imports Creative Sequencer publication packages, owns a manifest
repository, decodes AudioWMark candidates, resolves exact binding matches, and serves the browser
demo. It is not part of the DAW and does not claim complete C2PA SBR API conformance.

## Run

```sh
./scripts/setup_audiowmark.sh
python3 tools/softbinding-resolver/server.py
open http://127.0.0.1:8787
```

The server imports the default Sequencer outbox at startup. The browser's **Import Sequencer
Publications** action can repeat that idempotently. Repository state defaults to
`~/Library/Application Support/C2PA Soft Binding Demo/repository/`; use `--repository`,
`--outbox`, or `--audiowmark` to override paths.

## Routes

- `POST /manifests` stores `application/c2pa` bytes; pass the active identifier in
  `X-C2PA-Manifest-Id`.
- `POST /bindings` associates the experimental algorithm and 128-bit value with a manifest.
- `GET /matches/byBinding?algorithm=...&value=...` returns exact repository matches.
- `POST /matches/byContent` decodes uploaded audio and returns repository-backed candidates only.
- `GET /manifests/{manifestId}` returns the exact stored manifest bytes.
- `GET /services/supportedAlgorithms` reports the experimental supported identifier.
- `POST /imports/sequencer` imports the filesystem publication outbox.

AudioWMark can return false candidate patterns, so decoded output alone is never provenance.
Recovery requires exact repository membership and does not validate the derivative against the
original manifest's hard binding.

Run deterministic tests with:

```sh
python3 -m unittest discover tools/softbinding-resolver/tests
```
