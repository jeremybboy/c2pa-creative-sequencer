# C2PA SBR-Inspired Local Demonstration Service

One local resolver imports Creative Sequencer publications, owns one manifest repository, and exposes two distinct discovery pages. It is not part of the Sequencer and does not claim C2PA SBR conformance.

## Run from the Sequencer

Choose **Tools → Launch Recovery Demo**. If AudioWMark is not installed, approve the one-time
setup; after it succeeds the app starts this bundled server and opens the browser automatically.
The preferred port is 8787; if occupied, the app selects the next free port through 8797.
Use **Tools → Stop Recovery Demo** to stop it. The process also stops when the app quits.

## Command-line fallback

```sh
./scripts/setup_audiowmark.sh
./scripts/setup_audfprint.sh
python3 tools/softbinding-resolver/server.py
open http://127.0.0.1:8787
```

The landing page links to `/watermark` (embedded identifier plus exact lookup) and `/fingerprint` (derived landmarks plus similarity search). It also creates downloadable local MP3 test derivatives using fixed 320 kbps, 64 kbps, and 64 kbps plus 12 kHz low-pass presets. These are reproducible transformations, not claims about watermark recovery; test the same output on both recovery pages. Repository state defaults to `~/Library/Application Support/C2PA Soft Binding Demo/repository/`; override it with `--repository`, and override external tools with `--audiowmark`, `--audfprint`, or `--ffmpeg`.

The shared repository keeps `manifestId -> exact manifest bytes`, a watermark exact index, and a separate audfprint database/registration index. Old PR 011 packages and version-2 packages import idempotently.

Key routes are `POST /imports/sequencer`, `POST /matches/byContent/watermark`, `POST /matches/byContent/fingerprint`, `GET /matches/byBinding`, `GET /manifests/{manifestId}`, `GET /derivatives/presets`, `POST /derivatives`, and `GET /services/supportedAlgorithms`; the legacy `POST /matches/byContent` remains the watermark route. Uploaded derivative sources are processed in a temporary directory and are not added to the repository.

Fingerprint acceptance requires at least 10 time-aligned audfprint hashes. A decoded watermark candidate or generated fingerprint features alone never cause recovery. Any recovery is soft-binding association evidence, not validation of the derivative against the original hard binding.

Run normal deterministic tests with:

```sh
python3 -m unittest discover -s tools/softbinding-resolver/tests -v
```

The real WAV-to-MP3 integration is opt-in through CMake's `C2PASEQ_ENABLE_REAL_AUDIOWMARK_TEST` and `C2PASEQ_ENABLE_REAL_AUDFPRINT_TEST` options because normal CI does not require AudioWMark, audfprint, or FFmpeg.
