# Dependency Verification

Verified on 2026-09-11 on Apple silicon with macOS 26.6, Apple Clang 21.0.0, Xcode 26.4, Git 2.54.0, and CMake 4.3.2. This is an engineering dependency record, not legal advice.

## Decision summary

| Dependency | Current upstream checked | PR 001 selection | License finding | Compatibility finding |
|---|---|---|---|---|
| JUCE | 9.0.2 (`72782788`) | Tracktion's pinned JUCE 8.0.6 commit (`19edd538`) | AGPLv3 or commercial JUCE license | Tracktion Engine v3.2.0 pins this exact JUCE 8.0.6 commit; use the tested pair for PR 001 rather than assert unverified JUCE 9 compatibility |
| Tracktion Engine | v3.2.0 release (`0a5f4e6a`); `develop` reports 3.5.0 | v3.2.0 | GPLv3-or-later or commercial Tracktion license | Requires C++20 and supports macOS; its module API supplies `Engine`, device management, plug-in management, timeline/edit state, and rendering |
| VST 3 SDK | 3.8.1 build 84 (`3cdf9ca5`) | JUCE 8.0.6 bundled VST3 host interfaces in PR 008; standalone SDK integration remains deferred | MIT; Steinberg trademark/compatibility usage guidelines still apply | JUCE discovery, instantiation, editor, state, realtime processing, and offline render pass on Apple silicon with a deterministic VST3 fixture |
| `c2pa-cpp` | v0.26.9 (`26f7c8cd`), backed by `c2pa-rs` 0.90.15 | Integrated in PR 006 at v0.26.9 | MIT or Apache-2.0; transitive components require their own notices | Built on macOS arm64; WAV read, ingredient, ES256 sign/embed, reopen, validation, and tamper detection pass locally |

## Architecture consequences

1. Keep Tracktion Engine behind `TracktionAdapter`; its API surface must not dictate the rest of the application.
2. PR 001 uses the exact JUCE commit pinned by Tracktion Engine v3.2.0. JUCE 9.0.2 is newer, but compatibility with the stable Tracktion release was not established and is not required for the POC scaffold.
3. Keep every C2PA call inside `src/provenance`. The current API is context-oriented: use `c2pa::Context`, `c2pa::Reader`, `c2pa::Builder`, and `c2pa::Signer`; context-free reader/builder constructors compile but are deprecated.
4. Use JUCE/Tracktion plug-in hosting rather than coupling application code directly to Steinberg interfaces. PR 008 uses the VST3 interfaces bundled by the pinned JUCE 8.0.6 dependency; if a later distribution build adds the standalone SDK, test JUCE's custom-SDK path against the pinned MIT SDK.
5. Set the POC deployment target to macOS 13.3 because `c2pa-cpp` v0.26.9 sets that minimum and distributes an `aarch64-apple-darwin` prebuilt runtime.

## Deferred distribution checkpoint

This checkpoint is deliberately deferred while the software remains a private, local evaluation POC. It is not a blocker for implementation and should be reopened only before distribution, publication, or third-party use.

- A closed-source distributable requires appropriate commercial JUCE and Tracktion Engine licenses; they are separate products and one license does not cover the other.
- An open-source route would have to satisfy JUCE's AGPLv3 terms and Tracktion Engine's GPLv3-or-later terms together with every bundled/transitive notice. Do not choose this route casually.
- The current standalone VST 3 SDK source is MIT-licensed. Product naming, documentation, package, and compatibility claims remain subject to Steinberg's published trademark usage guidelines.
- `c2pa-cpp` is dual MIT/Apache-2.0. Its prebuilt runtime comes from `c2pa-rs`; preserve the transitive license inventory before distribution.
- Commercial license tier and seat counts depend on the owner, revenue/funding, developer count, and distribution model. Those facts are not available in this repository, so exact commercial cost is intentionally unresolved.

## API and feature evidence

### JUCE and Tracktion Engine

- Tracktion Engine v3.2.0's `CMakeLists.txt` adds JUCE as a module and requires C++20.
- The v3.2.0 tag pins JUCE commit `19edd538429c93d277bf95b55aaa7e3eb545f951`; that checkout declares JUCE 8.0.6.
- `tracktion::engine::Engine` constructs the project, device, render, audio-file, plug-in, and edit services and initializes the device manager through its behavior policy.
- JUCE's `VST3PluginFormat`, `AudioPluginFormatManager`, `AudioPluginInstance`, and editor APIs provide the required discovery, instantiation, state, processing, and UI-hosting surface for later VST slices.
- PR 008 enables that JUCE VST3 host surface behind `src/plugins`, while Tracktion Engine owns the one-effect-per-track execution node used by realtime playback and offline rendering. Application code does not include Steinberg interfaces directly.
- A deterministic arm64 test VST3 proves descriptor discovery, instantiation, parameter-state serialization/restoration, realtime signal change, Tracktion insertion/bypass/removal, missing-plug-in recovery, offline signal change, and C2PA-valid export without requiring third-party software in CI.
- The inspected `c2pa-audio-reference-product` revision `f8f88f3dbfa777e58137b77ab39dea60f50027f6` is a Swift/SwiftUI macOS 14 standalone app. Its own README explicitly says there are no VST3, Audio Unit, or CLAP targets yet and places VST3/AU integration on its future V3 roadmap, so it supplies no bundle that PR 008 can host.
- Upstream inconsistency: Tracktion's v3.2.0 files still expose some internal version strings as 3.1.0. Dependency reporting must use the pinned Git tag/commit rather than `Engine::getVersion()` until upstream resolves that mismatch.
- Tracktion v3.2.0 registers WAV and AIFF readers unconditionally. Its MP3 reader is present only when JUCE's `JUCE_USE_MP3AUDIOFORMAT` compile flag is enabled; PR 004 sets that flag to `1` and the engine test decodes an embedded MP3 fixture.
- Tracktion automatically enables tempo and pitch following for loop-tagged audio. This POC has no time-stretch backend enabled, so PR 004 explicitly normalises imported stems to native-speed, absolute-time playback; tempo matching remains out of scope.
- JUCE's MP3 decoder header carries a patent/non-infringement disclaimer. This does not block the private local POC, but it belongs in the deferred distribution review together with the JUCE and Tracktion license decision.

### `c2pa-cpp`

- CMake configuration selected `c2pa-v0.90.15-aarch64-apple-darwin.zip`, found the dynamic library, and built `libc2pa_cpp.dylib` successfully.
- Targeted upstream tests passed for supported MIME enumeration, signing without a timestamp authority, and reading an authenticated WAV from a stream.
- A separate local smoke test signed `sample1.wav` without a timestamp authority, embedded a manifest, reopened it with `c2pa::Reader`, confirmed a non-empty manifest, confirmed `audio/wav` in `Builder::supported_mime_types()`, and confirmed the result remained a normal RIFF/WAVE PCM file.
- PR 006 compiles the pinned C++ wrapper and ships the matching pinned `c2pa-rs` macOS runtime beside the app executable with a relative loader path. The arm64 runtime is locally validated; the official x86_64 archive is checksum-pinned to keep macOS CI viable but is not claimed as a local runtime validation. No credential is bundled.
- Normal app export requires C2PA signing. The first GUI export validates a user-selected PEM by constructing the signer, then copies it to the app's private Application Support directory with `0700` directory and `0600` file permissions; the environment variable remains a developer override only.
- The supplied C2PA Conformance test bundle contains one certificate and an EC private key in SEC1 PEM form. `c2pa-rs` 0.90.15 requires PKCS#8 for ES256; the application wraps the same key into PKCS#8 only in memory before constructing the signer.
- The supplied leaf certificate is issued by `C2PA Conformance Test Root`, but that root is not included in the supplied bundle. Asset/hash integrity validates locally; a verifier without the Conformance root can correctly report an external trust-list issue. Cryptographic integrity and trust-list recognition are separate results.
- Independent inspection of the PR 006 example with `c2patool` 0.26.30 reports `validation_state: Valid`, the expected `signingCredential.untrusted` trust-list issue, the C2PA Creative Sequencer 0.1.0 generator, and exactly the three audible deduplicated ingredients `A.wav`, `B.wav`, and `C.wav`.
- The full upstream CTest run was not a valid offline pass: 195 of 369 C++ tests passed, 174 tests attempted public timestamp or remote-manifest URLs and failed because network resolution was unavailable, and the separate C test executable had not been built. These failures do not contradict the isolated offline WAV result, but CI must split offline tests from explicit network tests.

## PR 001 build evidence

- CMake configured the selected Tracktion Engine v3.2.0/JUCE 8.0.6 pair with Apple Clang 21 using the Unix Makefiles generator.
- A second clean configure fetched immutable GitHub commit archives, verified their SHA-256 values, and reproduced the audited source files without using Tracktion's SSH submodule URL.
- The application and test executable built successfully as C++20; the app bundle contains a native arm64 Mach-O executable.
- CTest passed `application_skeleton` (1/1).
- The app launched as a persistent process, exposed a window titled `C2PA Creative Sequencer`, and exited cleanly after a standard application quit event.
- GitHub Actions subsequently built and tested PR 001 successfully with the Xcode generator on its hosted macOS runner.

## PR 004 audio-format evidence

- The pinned Tracktion/JUCE pair built with `JUCE_USE_MP3AUDIOFORMAT=1` on the supported Apple-silicon macOS target.
- The hosted-engine test generated and decoded WAV and AIFF fixtures, decoded an embedded MP3 fixture, completed a JUCE waveform thumbnail, inserted a Tracktion wave clip, and observed non-silent output from deterministic block processing.
- User-facing import still performs a real decoder open before any source is registered or copied; an accepted filename extension alone is not treated as valid audio.

## Primary sources

- [JUCE releases](https://github.com/juce-framework/JUCE/releases)
- [JUCE licensing](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)
- [Tracktion Engine repository and license](https://github.com/Tracktion/tracktion_engine)
- [Tracktion Engine v3.2.0](https://github.com/Tracktion/tracktion_engine/releases/tag/v3.2.0)
- [VST 3 SDK repository](https://github.com/steinbergmedia/vst3sdk)
- [VST usage guidelines](https://github.com/steinbergmedia/vst3_dev_portal/blob/main/src/pages/VST%2B3%2BLicensing/Usage%2Bguidelines.md)
- [`c2pa-cpp` repository](https://github.com/contentauth/c2pa-cpp)
- [`c2pa-cpp` v0.26.9](https://github.com/contentauth/c2pa-cpp/releases/tag/v0.26.9)
- [C2PA supported formats](https://opensource.contentauthenticity.org/docs/c2pa-node/docs/supported-formats/)
