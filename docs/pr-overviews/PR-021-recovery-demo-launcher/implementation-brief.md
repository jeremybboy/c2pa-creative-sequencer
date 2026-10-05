# PR 021 — Recovery demo launcher

## Goal

Remove routine Terminal setup from the AudioWMark demonstration while keeping the external tools,
licensing boundary, local resolver, and explicit user consent honest.

## Verified starting point

- Audio soft-binding export and the local recovery service already work through repository scripts.
- AudioWMark is an external GPL executable installed under Application Support, not linked into the app.
- Users must currently run setup and server commands manually.

## Change ledger

| Area | Before | PR 021 target |
| --- | --- | --- |
| AudioWMark setup | Run a shell script in Terminal | Native Tools action and Audio SB first-use prompt |
| Consent | Terminal command is the consent boundary | Explicit dialog describes download, build tools, location, and GPL boundary |
| Recovery demo | Start Python manually and open a URL | One action starts the bundled server and opens the browser |
| Test derivative | Run an FFmpeg helper from Terminal | Landing-page presets create a downloadable local MP3 |
| Lifecycle | User stops the command-line process | Stop action and automatic shutdown when the app quits |
| Packaging | Server and setup live only in the checkout | Required scripts and static assets are copied into the signed app bundle |
| Developer access | Repository scripts | Preserved as documented fallbacks |

## Architecture and constraints

- The app launches pinned repository tooling as child processes; it does not link AudioWMark or
  move decoding into the realtime audio path.
- The AudioWMark runtime remains machine-local under Application Support and outside projects/Git.
- The resolver remains a local, non-conformant demonstration service on `127.0.0.1:8787`.
- If 8787 is occupied, the app selects the next free local port through 8797 without terminating
  the unknown listener.
- Compatibility checks require both the recovery algorithms endpoint and the derivative-preset
  endpoint, so an older demo server is skipped instead of serving a partially updated interface.
- Startup and bounded HTTP readiness checks run off the message thread, so a stale listener cannot
  freeze the Sequencer UI.
- Python bytecode writes are disabled so launching the server does not mutate the signed app bundle.
- A first launch can install AudioWMark with consent and continue directly into the demo.
- The landing page exposes fixed 320 kbps, 64 kbps, and 64 kbps plus 12 kHz low-pass presets;
  labels describe transformations and never predict watermark recovery.
- Source uploads and generated MP3s use a temporary directory and are not retained by the resolver.
- FFmpeg is reused from the audfprint setup record, PATH, or a standard Homebrew location; the UI
  disables conversion and explains setup when no executable is available.
- Quitting the Sequencer terminates only the recovery process it launched.

## Explicitly unchanged / out of scope

- No audfprint installer, cloud service, authentication, background daemon, or automatic publishing.
- No changes to watermark strength, fingerprint thresholds, C2PA assertions, export, or audio DSP.
- No general settings framework or redesign of the toolbar.

## Verification plan

Automated tests cover bundle-path resolution, refusal to launch when required packaged/runtime
files are absent, non-blocking recovery startup, derivative routes, and exact preset commands. Build verification must confirm the complete Release suite, bundled resources, and
strict app-bundle code signature. Human acceptance must confirm the native Tools menu, one-action
browser launch, downloadable preset output, successful import/recovery with an existing publication,
Stop behavior, and server shutdown on app exit. The destructive missing-runtime install path is not required on a machine that
already has a verified AudioWMark runtime.

## Acceptance state

- The Release application and all 17 tests pass locally.
- Bundled resolver assets, absence of bundle-local Python bytecode, and the strict app signature are verified.
- A local UI smoke test verified the native Tools menu, browser launch, live resolver endpoint, Stop action, and automatic server shutdown when the app closes.
- Independent human acceptance is pending before commit/push/PR publication.
