# C2PA SBR-Inspired Local Demonstration Service

One local resolver imports Creative Sequencer publications, owns one manifest repository, and exposes two distinct discovery pages. It is not part of the Sequencer and does not claim C2PA SBR conformance.

## Run

```sh
./scripts/setup_audiowmark.sh
./scripts/setup_audfprint.sh
python3 tools/softbinding-resolver/server.py
open http://127.0.0.1:8787
```

The landing page links to `/watermark` (embedded identifier plus exact lookup) and `/fingerprint` (derived landmarks plus similarity search). Repository state defaults to `~/Library/Application Support/C2PA Soft Binding Demo/repository/`; override it with `--repository`, and override external tools with `--audiowmark` or `--audfprint`.

The shared repository keeps `manifestId -> exact manifest bytes`, a watermark exact index, and a separate audfprint database/registration index. Old PR 011 packages and version-2 packages import idempotently.

Key routes are `POST /imports/sequencer`, `POST /matches/byContent/watermark`, `POST /matches/byContent/fingerprint`, `GET /matches/byBinding`, `GET /manifests/{manifestId}`, and `GET /services/supportedAlgorithms`; the legacy `POST /matches/byContent` remains the watermark route.

Fingerprint acceptance requires at least 10 time-aligned audfprint hashes. A decoded watermark candidate or generated fingerprint features alone never cause recovery. Any recovery is soft-binding association evidence, not validation of the derivative against the original hard binding.

Run normal deterministic tests with:

```sh
python3 -m unittest discover -s tools/softbinding-resolver/tests -v
```

The real WAV-to-MP3 integration is opt-in through CMake's `C2PASEQ_ENABLE_REAL_AUDIOWMARK_TEST` and `C2PASEQ_ENABLE_REAL_AUDFPRINT_TEST` options because normal CI does not require AudioWMark, audfprint, or FFmpeg.
