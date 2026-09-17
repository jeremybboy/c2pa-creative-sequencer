# PR 006 Export and Content Credentials Acceptance

## Build and launch

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open "build/C2PACreativeSequencer_artefacts/Debug/C2PA Creative Sequencer.app"
```

Launch normally without a signing environment variable. The supplied C2PA Conformance
credential is a test identity only, not production signing material.

## Required workflow

1. Create or open a project and import two ordinary WAV files.
2. Arrange audible clips on two tracks, including one same-track overlap and initial silence.
3. Import the known C2PA WAV fixture and confirm the clip automatically shows **CC**; click **Credentials** for details.
4. Confirm ordinary sources say **No Content Credentials** and the signed source exposes an active manifest.
5. Play from time zero and confirm timing, same-track priority, and cross-track mixing.
6. Mute a track or delete one clip so its media must not contribute to export.
7. Click **Export**. On first use, choose the supplied PEM once; then select a `.wav` destination and wait for the signed/validated result.
8. Reopen the output in an audio player and confirm it remains a normal stereo WAV.
9. Import the output into a new project and use **Credentials** to confirm its manifest is present and protected asset data validates.
10. Quit and relaunch the app normally, export again, and confirm no credential prompt appears.
11. Verify the output with `c2patool` and Conformulator, and confirm it remains playable.
12. Confirm its manifest names only unique audible contributing sources; the muted/deleted source must be absent.

Conformulator must detect and display the manifest. A test certificate may still report
`signingCredential.untrusted` when its root is absent from the verifier's trust store; that
trust-list result is distinct from manifest presence, signature validity, and asset integrity.

Remove the credential through **Signing**, repeat Export, and confirm the app asks for setup
instead of creating an unsigned WAV. Do not approve if a failed signing attempt leaves a
destination presented as authenticated, if render extends beyond the last audible clip, or if
source selection disagrees with the final audible arrangement.

## PR 011 AudioWMark authoring and external recovery acceptance

### Part A — authoring

1. Run `scripts/setup_audiowmark.sh`, launch the Sequencer, and confirm **Audio SB** reports ready.
2. Export the same arrangement once with **Audio SB** off and once on. While enabled export runs,
   move the app window and confirm the UI stays responsive; cancellation must return cleanly.
3. Confirm the enabled WAV is playable, stereo, 24-bit, has the expected sample rate/duration,
   and its embedded C2PA validates.
4. Confirm the manifest contains `c2pa.watermarked.bound`, the C2PA 2.4 `c2pa.soft-binding`
   blocks schema, `io.github.jeremybboy.audiowmark.1`, and a 32-character lowercase value.
5. Confirm the matching `SoftBindingOutbox/<binding-id>/` contains `binding.json` and
   `manifest.c2pa`, but no audio, project media, or signing credential.
6. Listen comparatively to the normal and watermarked exports. Human listening quality remains
   open until explicitly accepted; automated tests are not perceptual acceptance.

### Part B — external resolver

7. Run `python3 tools/softbinding-resolver/server.py` and open `http://127.0.0.1:8787`.
8. Click **Import Sequencer Publications**; repeating the import must be idempotent.
9. Create a manifestless derivative with
   `python3 scripts/make_softbinding_demo_derivative.py signed.wav derivative.wav`.
10. Drop it into the browser. Confirm no embedded C2PA is reported, AudioWMark finds the exact
    128-bit repository value, and the exact stored `.c2pa` is available.
11. Confirm the UI says **Content Credentials recovered via audio watermark** and explicitly says
    the derivative has not passed the original asset's cryptographic hard binding.
12. Drop an unrelated unwatermarked WAV and confirm **No matching soft-bound Content Credentials
    found**, with no provenance claim and no crash.

The opt-in real integration test covers real embed, C2PA signing, metadata-only derivative,
external decode, exact repository match, idempotent import, and byte-exact manifest retrieval.
