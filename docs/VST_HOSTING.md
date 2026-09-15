# Constrained VST3 Hosting

PR 008 adds one VST3 **audio effect** slot per audio track on Apple-silicon macOS.
It deliberately does not add effect chains, a master slot, AU/AAX, instruments,
MIDI, automation, sidechains, presets, latency UI, or plug-in sandboxing.

## Discovery and controls

Use the **+ VST** button in a track header, then choose **Scan VST3**. The explicit
scan inspects:

- `~/Library/Audio/Plug-Ins/VST3`
- `/Library/Audio/Plug-Ins/VST3`

Results are cached at
`~/Library/Application Support/C2PA Creative Sequencer/vst3-cache.xml`; startup
loads this metadata but does not rescan the system. The track menu lists scanned
audio effects and provides Open, Bypass/Enable, and Remove for the selected slot.
Instrument entries are not loadable in this milestone.

The editor window uses the plug-in's editor when one is provided and otherwise
uses JUCE's generic parameter editor. The project stores opaque parameter state,
identity, metadata, and bypass state, but never copies a third-party bundle.

Closing an editor destroys it. Arrangement mutations currently rebuild the Tracktion
Edit, so the host also closes all open editor windows before Tracktion destroys their
processor instances. This preserves the POC architecture while preventing stale
editor timers or callbacks from accessing deleted processors; reopen the editor from
the track menu after the arrangement edit if it is still needed.

## Audio path

The Tracktion Engine graph is:

```text
source clips → arrangement trim/occlusion → track VST3 → track gain/pan/mute/solo → master
```

Both live playback and offline stereo 24-bit WAV rendering use that graph. Normal
Export then runs the unchanged mandatory C2PA pipeline: render, build claim, sign,
embed, reopen, and validate. PR 008 does not describe the plug-in, its parameters,
or AI use in the manifest; that semantic work is deferred to PR 009.

## Persistence and failure behavior

Save captures the live plug-in state blob and bypass flag. Reopening restores the
same identified bundle and state. If the bundle is absent or initialization fails,
the project still opens, the track and clips remain usable, the saved identity and
state are retained as **Missing Plugin: _name_**, and processing remains bypassed.
No alternate plug-in is substituted.

A third-party plug-in still runs in the sequencer process. A plug-in crash can take
down the app; process sandboxing is explicitly deferred. Compatibility is therefore
POC-level, not a claim that arbitrary commercial plug-ins work.

## Reference plug-in status

The repository `jeremybboy/c2pa-audio-reference-product` was inspected at commit
`f8f88f3dbfa777e58137b77ab39dea60f50027f6` on 2026-09-14. It currently builds a
macOS 14 Swift/SwiftUI standalone Loop Generator and explicitly reports no VST3,
Audio Unit, or CLAP target; VST3/AU is a future roadmap item. Consequently there is
no AI-reference VST3 bundle, bundle output path, plug-in audio I/O contract, or
plug-in runtime/assets contract for this host to test. Integration requires that
external repository to provide an Apple-silicon VST3 audio-effect bundle plus its
build and runtime-asset instructions; its source must not be copied here.

Surge XT was not forced into this milestone: it is an instrument, and proving it
would require introducing a MIDI/note-source architecture outside this sequencer's
fixed audio-stem scope. A deterministic stereo gain-effect VST3 is built only for
tests and proves the host without depending on installed plug-ins or commercial CI
software.

## Verified PR 008 evidence

- The deterministic `C2PA Test Gain` VST3 was discovered with its descriptor,
  instantiated at 48 kHz/stereo, changed a unity realtime buffer to exactly 0.25,
  restored a serialized 0.60 gain state into a fresh instance, and produced an
  offline Tracktion render at 0.25 of its bypassed RMS.
- A normal C2PA export with the fixture enabled contained the 0.25 processed PCM,
  reopened with a present/intact C2PA manifest, and restored the hosted plug-in on
  project reopen. The existing ingest/export regression suite remained green.
- Two installed external arm64 effect bundles were exercised outside CI:
  `Transparent Neural Reverb` 0.1.0 by Transparent Audio POC and `C2PA` 1.0.0 by
  Jeremy Uzan. Each was discovered, instantiated, exposed an editor, returned a
  non-empty state blob, loaded into Tracktion's track graph, and completed a
  non-silent offline render. This is execution evidence, not a human audibility
  claim or proof that either is the unavailable reference-product VST3.
- In the built app, an explicit standard-location scan cached six VST3 descriptors.
  Transparent Neural Reverb loaded on Track 1 and its editor window opened. Toggling
  bypass while Play was active left the transport playing and advanced the playhead
  from 1.525 s to 3.723 s instead of resetting it.
- The installed Melodyne 5.4.2 bundle failed macOS initialization with error
  `-67061`; the host reported no compatible effect instead of treating it as a
  successful load. The installed Co-Producer bundle crashed the unsandboxed test
  host during its WebKit initialization. These are concrete examples of why broad
  compatibility and process sandboxing are not claimed in this POC.

The external smoke checks above did not include a source clip or human listening in
the app. Audible bypass difference, parameter feel, save/quit/reopen through the UI,
and external-verifier confirmation remain manual acceptance steps before merge.

## Manual acceptance

1. Put audio on a track and use **+ VST → Scan VST3**.
2. Select a compatible VST3 audio effect and press Play.
3. Toggle Bypass during playback; verify the audible mix changes without a playhead reset.
4. Open the editor, change a parameter, save, quit, and reopen the project.
5. Verify the slot and parameter state restore, export, and compare the rendered audio.
6. Reimport the WAV and open Credentials; verify the manifest is detected and valid.

The deterministic test fixture proves signal and C2PA behavior mechanically. A
human listening pass with a real external effect remains required before merge.
