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

1. Launch the Sequencer and enable **Audio SB**. If AudioWMark is absent, approve its one-time setup and wait for completion; `scripts/setup_audiowmark.sh` remains the command-line fallback.
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

7. Choose **Tools → Launch Recovery Demo**; confirm the Sequencer remains responsive while the browser opens the selected local URL (normally `http://127.0.0.1:8787`, or the next responsive/free port through 8797). The command-line fallback is `python3 tools/softbinding-resolver/server.py`.
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

## PR 012 fingerprint and MP3 acceptance

1. Install AudioWMark through **Tools → Install AudioWMark…** (or enable **Audio SB** and approve setup), then run `scripts/setup_audfprint.sh`.
2. Launch the app, enable **Audio SB** and **FP SB**, then export a signed WAV. Confirm the app remains responsive through **Computing audio fingerprint**.
3. Verify the WAV's C2PA, then inspect its outbox package: `manifest.c2pa`, `binding.json`, `fingerprint.json`, and `fingerprint-data.afpt` must exist; no WAV, project media, PEM, or key may exist there.
4. Confirm the manifest has separate AudioWMark and audfprint `c2pa.soft-binding` assertions, one `c2pa.watermarked.bound` action, and no fingerprint action.
5. Choose **Tools → Launch Recovery Demo**, import once and again, and confirm the second import is idempotent. Check `/watermark` and `/fingerprint` from the same process; **Tools → Stop Recovery Demo** or quitting the app must stop the server.
6. On the recovery-demo landing page, choose the signed WAV under **Create Test Derivative**, select **MP3 64 kbps + 12 kHz low-pass**, and download the result. This fixed preset reproduces the measured PR 013 transformation and strips metadata. Submit that same MP3 to both pages. The command-line fallback remains `python3 scripts/make_mp3_demo_derivative.py signed.wav derivative.mp3`.
7. Fingerprint acceptance requires at least 10 aligned hashes and must show the actual evidence. The manually verified PR 013 fixture produced 49 aligned / 200 raw common hashes, 177 query hashes, 27.68% coverage, and 6.36 seconds of support; it recovered the correct manifest.
8. On that same MP3, AudioWMark decoded eight candidates but none matched the registered 128-bit value. Report this as "the registered watermark identifier was not recoverable by the configured decoder," not as proof that the watermark was physically removed or destroyed.
9. Submit unrelated audio to `/fingerprint`; the verified deterministic noise control produced 678 query hashes, zero matches, and no provenance recovery.

The landing page also offers 320 kbps and plain 64 kbps MP3 presets for comparison. These labels
describe only the FFmpeg operation; they do not claim that a watermark was removed, destroyed, or
will be unavailable. The recovery pages must report the measured decoder and lookup results for
each generated file. FFmpeg is discovered from the fingerprint runtime record, the process PATH,
or standard Homebrew locations; if unavailable, the converter stays disabled with setup guidance.

For automated opt-in acceptance, configure with both `C2PASEQ_ENABLE_REAL_AUDIOWMARK_TEST=ON` and `C2PASEQ_ENABLE_REAL_AUDFPRINT_TEST=ON`, then run the `real_mp3_soft_binding_pipeline` CTest. Normal CI remains independent of both runtimes and FFmpeg.

## PR 023 credentialed MIDI stem acceptance

1. Open a project, add a MIDI track, create a clip away from bar 1, and draw several notes.
2. Load a VST3 instrument (for example, your installed Surge XT), choose a recognizable sound,
   and confirm playback. Set gain/pan so a change in level or position would be noticeable.
3. Configure the normal C2PA signer through **Signing** if needed. No new credential is required.
4. Right-click the MIDI track header, or open its instrument menu, and choose
   **Bounce to Credentialed Audio Stem**. With no notes, instrument, or signer, the item is disabled.
5. Confirm a new **<track name> Rendered Stem** audio track appears, its audio starts where the
   MIDI clip starts, source MIDI is muted, and gain/pan are retained on the audio track.
   Playback is paused; press Play to compare timing, level, pan, and the chosen instrument sound.
6. Select the bounced waveform and click **Credentials**. Confirm embedded credentials, integrity
   validation, `c2pa.created` / `c2pa.rendered`, the actual track/BPM/counts, and instrument identity.
   External trust may still report an issue for the test certificate; it is not an integrity failure.
7. Undo once: the stem track should disappear and source mute/solo should return. Redo once:
   the stem and mute should return. Save, close, and reopen; verify both audio and credentials.
8. Export the final mix and choose **View Credentials**. Confirm the signed bounced WAV is a
   credentialed ingredient alongside any other audible source media. Detailed stem actions live
   in the ingredient's own manifest, not a fabricated final-mix MIDI ingredient.
9. Try an unrelated soloed audio track and a muted MIDI source: bounce must still render only
   the selected instrument. If the MIDI source itself was soloed, solo should move to the stem.
10. On a longer bounce, press **Cancel**; wait for the current stage to finish and confirm no new
    track or source mute. Normal Quit should ask you to wait/cancel while the worker is active.

The stem is rendered before track gain/pan and without master processing; its new audio track
inherits the mixer settings so they are applied only once. Rendering stops at the last non-empty
MIDI clip end; allow space in the clip for releases/reverb. Notes remain editable on the muted MIDI
track, but later edits or BPM changes do not regenerate or stretch an existing bounced WAV.
No recording, MIDI-file import, soft binding on stems, or automatic instrument-tail detection
is part of this PR. Automated acceptance uses the repository test synth; your instrument/preset
and listening acceptance still need this manual check.

## Automatic MIDI provenance export acceptance

1. Open or create a project with imported audio and an **unmuted MIDI track** containing notes
   and a loaded VST3 instrument. Do not use the bounce command. Put the MIDI clip away from
   bar one to check alignment, and choose an easily recognizable sound in Surge XT or your synth.
2. Change MIDI track gain/pan and a synth parameter/preset. Confirm playback; configure signing.
3. Press **Export** normally. Expect progress for MIDI stem rendering/signing, followed by the
   normal final-mix export. There is no extra checkbox. Instrument editors close for offline work.
4. Confirm no new tracks or audio clips appeared, no source MIDI mute/solo changed, and no new
   undo action appeared. Playback is paused at its previous position; press Play to confirm the
   original MIDI/instrument remains playable with its chosen sound.
5. Listen to the exported WAV: check MIDI timing, stereo pan, level, and imported audio against
   playback. Choose **View Credentials** after export: expect an automatic MIDI-stem count and
   an actual credentialed `<track name> Stem.wav` ingredient with MIDI and instrument metadata.
   Its created/rendered history is embedded in the final manifest, not a fabricated source entry.
6. Try two MIDI tracks: expect two stems. Mute one, or solo the other: expect only the eligible
   audible track's stem. A manually bounced, muted MIDI source should not create a duplicate
   automatic ingredient; its audible signed audio stem is still a normal ingredient.
7. Try **Cancel** during a longer export; wait for the current stage, then confirm no new output
   replaced an existing destination and MIDI still plays. Do not force-quit during rendering;
   normal Quit asks you to wait. Save/reopen afterwards and confirm the original arrangement.

No intermediate WAVs are added to the project. Manual bounce remains useful when you want to
keep a separate signed stem. Releases/reverb beyond MIDI clip ends are not automatically added;
extend the clip beforehand. Automated tests use the test synth, not proof of your Surge XT preset.

## PR 026 arrangement editing and UI refinement acceptance

Use the Release app from `build-pr021` (the folder name is historical), built from
`codex/arrangement-editing-ui-refinement`. Quit the previous app instance before launching it.

1. Open a project with Audio/VST3 and MIDI/instrument tracks. Check the taller track-header
   controls, readable proportions, long-name ellipsis/full-name tooltip, gain/pan, M/S, arm and input.
2. Open a MIDI clip; check C, C#, D, F#, A#, B row/note labels at normal zoom. MIDI 60 is still C4.
   At dense pitch zoom, only C anchors remain; hover still identifies the exact musical pitch.
3. Create, hover, select and drag notes: check musical names in the header/note body and creation
   status. Move/resize, velocity, clipboard and repeated Command-D must remain functional.
4. Pinch the arrangement or Command-scroll farther in. Watch **Grid** in the status line: bars,
   beats, 1/2, 1/4, then 1/8 beat. At high zoom, trim a short section and check the visible grid
   predicts the result. Option still bypasses snap; BPM changes alter musical step duration.
5. Drag across the waveform body in the middle of a sample or recorded take (not its move header),
   then press **Delete** or **Backspace**. Dragging empty lanes also selects time across tracks.
6. Confirm two fragments and a hole remain: later audio does not move. Play across the gap,
   undo/redo, then save/reopen and check identical timing and unchanged source credentials.
7. Select ranges overlapping the beginning, end, and whole clip: confirm left trim, right trim,
   and removal. An outside/empty selection changes no project audio; MIDI time ranges are untouched.
8. Open **Input**: smaller device groups appear first; devices with >8 channels are under
   **Advanced inputs**, not discarded. Select a channel, reopen the menu and check its selected
   entry/tooltip. This is session state; no physical/virtual certification or hardware persistence.
9. Repeat mono recording/Stop/sign/import, manual MIDI bounce, and normal signed Export with
   imported audio + live MIDI + recorded fragments. Inspect credentials: fragments refer to the
   same original signed media, automatic MIDI provenance still appears, and the mix validates.

Range delete is non-destructive: no new audio, import or C2PA claim is created by trim/split/delete.
Only clip placement/source offsets/duration/fragment IDs change. Recording still signs after
stop/finalize, and final Export creates final media credentials. No ripple, destructive editing,
new action templates, monitoring, comping, crossfades, pitch editing, or cloud scope.
Offscreen renders and synthetic/regression tests are not live hardware or listening acceptance.

## PR 025 minimal credentialed audio recording acceptance

1. Connect an input (microphone or audio interface), create/open a project, and configure the
   existing signer through **Signing**; the supplied credential is a test identity only.
2. On an **Audio** track, click **Input** and select a device/channel, then click its red-circle
   **Arm** control. Allow macOS microphone access if asked. Arming a second Audio track disarms
   the first; MIDI tracks have no audio-arm control. Inputs stay closed at normal startup.
3. Speak/play into the selected input and confirm the armed track's meter moves even with
   transport stopped. **Input\*** and its tooltip identify the input-meter mode. No input audio
   is sent to the speakers; use interface direct monitoring if needed, not software monitoring.
4. Turn **Loop** off and put the playhead away from zero. Press the global red-circle **Record**
   beside Play/Stop. Transport starts or continues; status says **RECORDING**. Project editing
   is locked for the take. No clip or provenance is created merely by arming or metering.
5. Press **Stop**, **Record** again, or **Space** to finalize. Wait for signing/validation, then
   confirm one new clip appears on the original armed track at the recording start position.
   Playback is paused; press Play to listen and compare placement against existing material.
6. Select that clip and click **Credentials**. Inspect `c2pa.created` with `digitalCapture` and
   `c2paseq:audioCapture`: actual input label/index, track name, sample rate, mono channel count,
   24-bit depth, frame count, start/end/duration, take ID, and application name/version.
   These fields are capture evidence, **not proof of performer or microphone identity**.
7. Export normally and view the mix credentials: the recorded signed WAV must be a normal
   ingredient with its own embedded capture manifest. Include a MIDI instrument track and
   confirm PR 024 automatic MIDI provenance still works without manual bounce.
8. Undo/redo the take import, then save/reopen and confirm the clip, sound, and credentials remain.
   Undo/redo, track deletion, and project switching disarm input; explicitly rearm for another take.
9. Record another take and click **Cancel** or **Escape** before Stop: confirm no clip/media import.
   With no signer, Record is disabled. Device interruption or signing failure must report rejection,
   never import unsigned audio. Finalization cannot be cancelled mid-signature; wait before quitting.

Automated acceptance uses synthetic samples through the same callback queue and real C2PA signing;
live input enumeration, macOS permission, audible quality, and physical-device placement still require
the steps above. Capture is raw mono input, before track effect/gain/pan; normal playback/export
applies the track processing. Timing is not latency-compensated. No software monitoring, loop/punch
recording, comping, MIDI recording, cloud work, persistent stem cache, or production credentialing.
