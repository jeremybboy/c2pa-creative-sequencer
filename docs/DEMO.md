# PR 005 Arrangement Acceptance

This is the required hands-on gate for PR 005. Automated tests cover model timing,
native-speed playback plumbing, edits, and persistence; they do not prove mouse feel,
screen hierarchy, or what reaches your speakers.

## Build and launch

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
open "build/C2PACreativeSequencer_artefacts/Debug/C2PA Creative Sequencer.app"
```

Use WAV for the first listening pass because WAV is the canonical input for later
render and provenance work.

## Required workflow

1. Launch the application.
2. Click **Add Folder…** under Places.
3. Select a folder containing audio samples.
4. Expand that folder and a nested subfolder inside the application.
5. Drag one sample onto Track 1 at bar 1.
6. Drag a second sample onto Track 1 so that it overlaps the first sample.
7. Drag a third sample onto Track 2 at bar 3.
8. Confirm each clip width represents its real duration.
9. Confirm every clip edge aligns with the numbered ruler and grid.
10. Press **Space** to play, then press it twice to pause and resume at the same position.
11. Hear the newer Clip 2 replace Clip 1 only in their Track 1 overlap; Clip 1 must
    remain audible before and after it, while Track 2 can play simultaneously.
12. Drag the bar-3 clip horizontally to bar 6.
13. Replay and hear it begin at bar 6.
14. Drag that clip vertically to another track and confirm it stays wholly within the lane.
15. Drag both clip edges to trim its source range.
16. Select it and press **Command-D** to duplicate it.
17. Select the duplicate and press **Delete**.
18. Double-click track names and rename them, for example Vocal, Kick, and Bass.
19. During playback, enable **S** on one track and verify only soloed material is heard
    without playback stopping or the playhead moving backward.
20. During playback, enable **M** on another track and verify only the mix changes;
    transport and playhead must continue uninterrupted.
21. Use **−/+** or Command-scroll to zoom horizontally.
22. Shift-scroll horizontally and ordinary-scroll vertically; verify lanes and right headers remain aligned.
23. Press **Command-S**, then also verify the **Save** button uses the same save behavior.
24. Quit the app.
25. Relaunch and open the same `.c2paseq` folder.
26. Verify track names/state, clip tracks/positions/trims, zoom, waveforms, and audible timing match the saved arrangement.

Move or delete the newer overlapping clip and verify the previously covered portion of
the older clip becomes audible again. Also verify that holding **Option** during a clip
drag bypasses beat snapping, **Command-E** splits the selected clip only when the playhead
is inside it, and **Command-Z** / **Shift-Command-Z** undo and redo edits. Right-click a root in Places and choose
**Remove from Places**; the source folder and all source files must remain untouched.

Do not merge the PR if same-track overlaps sum, Mute/Solo interrupts transport, Space
resets the playhead, visible placement disagrees with playback timing, a clip can sit
between lanes, headers drift from lanes during scrolling, source duration/pitch changes,
or save/reopen changes the arrangement.
