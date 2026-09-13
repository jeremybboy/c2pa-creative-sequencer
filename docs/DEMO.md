# PR 004 Audio Import Check

This is a focused engineering check, not the final provenance demo.

1. Build and open the Debug app using the commands in the README.
2. Click **New** and create a `.c2paseq` project folder.
3. Drag one WAV file onto the arrangement, or click **Import Audio** and choose a WAV, AIFF, or MP3 file.
4. Confirm that a waveform appears on a new track and that the status line reports one imported audio stem.
5. Press **Play** and listen through the selected output device; press **Stop** to return to zero.
6. Save, quit, reopen the project, and confirm that the waveform and playback return.
7. Compare the SHA-256 of the original file and its copy under the project's `Media/` directory; they must match.

WAV is the canonical manual check because later render and C2PA slices target WAV. Import success proves readable audio, a byte-preserving project copy, model registration, clip placement, cached waveform generation, and Tracktion playback; it does not prove C2PA inspection or signing.

For a loop-tagged WAV, confirm that playback keeps the file's native duration and
pitch even when its embedded BPM differs from the project BPM. PR 004 intentionally
does not provide clip warping or tempo matching.
