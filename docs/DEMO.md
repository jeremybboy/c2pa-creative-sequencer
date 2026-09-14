# PR 006 Export and Content Credentials Acceptance

## Build and launch with test signing

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
export C2PASEQ_SIGNING_BUNDLE_PEM="/absolute/path/to/test-signing-bundle.pem"
open "build/C2PACreativeSequencer_artefacts/Debug/C2PA Creative Sequencer.app"
```

This is the C2PA Conformance test identity, not production signing material.

## Required workflow

1. Create or open a project and import two ordinary WAV files.
2. Arrange audible clips on two tracks, including one same-track overlap and initial silence.
3. Import the known C2PA WAV fixture if available and click **Credentials** on selected clips.
4. Confirm ordinary sources say **No Content Credentials** and the signed source exposes an active manifest.
5. Play from time zero and confirm timing, same-track priority, and cross-track mixing.
6. Mute a track or delete one clip so its media must not contribute to export.
7. Click **Export**, select a `.wav` destination, and wait for the signed/validated result.
8. Reopen the output in an audio player and confirm it remains a normal stereo WAV.
9. Import the output into a new project and use **Credentials** to confirm its manifest is present and protected asset data validates.
10. Confirm its manifest names only unique audible contributing sources; the muted/deleted source must be absent.

Without the environment variable, repeat Export and confirm the app clearly reports an
unsigned WAV. Do not approve if a failed signing attempt leaves a destination presented as
authenticated, if render extends beyond the last audible clip, or if source selection
disagrees with the final audible arrangement.
