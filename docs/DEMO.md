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
