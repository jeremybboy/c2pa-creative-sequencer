# Dependency Verification

Verified on 2026-09-11 on Apple silicon with macOS 26.6, Apple Clang 21.0.0, Xcode 26.4, Git 2.54.0, and CMake 4.3.2. This is an engineering dependency record, not legal advice.

## Decision summary

| Dependency | Current upstream checked | PR 001 selection | License finding | Compatibility finding |
|---|---|---|---|---|
| JUCE | 9.0.2 (`72782788`) | Tracktion's pinned JUCE 8.0.6 commit (`19edd538`) | AGPLv3 or commercial JUCE license | Tracktion Engine v3.2.0 pins this exact JUCE 8.0.6 commit; use the tested pair for PR 001 rather than assert unverified JUCE 9 compatibility |
| Tracktion Engine | v3.2.0 release (`0a5f4e6a`); `develop` reports 3.5.0 | v3.2.0 | GPLv3-or-later or commercial Tracktion license | Requires C++20 and supports macOS; its module API supplies `Engine`, device management, plug-in management, timeline/edit state, and rendering |
| VST 3 SDK | 3.8.1 build 84 (`3cdf9ca5`) | Integration deferred to PR 013; pin 3.8.1 build 84 when added | MIT; Steinberg trademark/compatibility usage guidelines still apply | Upstream lists Apple silicon, macOS 10.14–26, and Xcode 10–26.5; JUCE exposes VST3 host discovery and native editor APIs |
| `c2pa-cpp` | v0.26.9 (`26f7c8cd`), backed by `c2pa-rs` 0.90.15 | Integration deferred to PR 018; pin v0.26.9 | MIT or Apache-2.0; transitive components require their own notices | Built on macOS arm64; reader, builder, signer, ingredient, manifest, and supported-format APIs are present; WAV read/sign/embed/reopen passed locally |

## Architecture consequences

1. Keep Tracktion Engine behind `TracktionAdapter`; its GPL/commercial terms and API surface must not dictate the rest of the application.
2. PR 001 uses the exact JUCE commit pinned by Tracktion Engine v3.2.0. JUCE 9.0.2 is newer, but compatibility with the stable Tracktion release was not established and is not required for the POC scaffold.
3. Keep every C2PA call inside `src/provenance`. The current API is context-oriented: use `c2pa::Context`, `c2pa::Reader`, `c2pa::Builder`, and `c2pa::Signer`; context-free reader/builder constructors compile but are deprecated.
4. Use JUCE/Tracktion plug-in hosting rather than coupling application code directly to Steinberg interfaces. When PR 013 adds the SDK, test JUCE's custom-SDK path against the pinned MIT SDK rather than relying silently on JUCE 8's older bundled copy.
5. Set the POC deployment target to macOS 13.3 because `c2pa-cpp` v0.26.9 sets that minimum and distributes an `aarch64-apple-darwin` prebuilt runtime.

## Licensing checkpoint

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
- Upstream inconsistency: Tracktion's v3.2.0 files still expose some internal version strings as 3.1.0. Dependency reporting must use the pinned Git tag/commit rather than `Engine::getVersion()` until upstream resolves that mismatch.

### `c2pa-cpp`

- CMake configuration selected `c2pa-v0.90.15-aarch64-apple-darwin.zip`, found the dynamic library, and built `libc2pa_cpp.dylib` successfully.
- Targeted upstream tests passed for supported MIME enumeration, signing without a timestamp authority, and reading an authenticated WAV from a stream.
- A separate local smoke test signed `sample1.wav` without a timestamp authority, embedded a manifest, reopened it with `c2pa::Reader`, confirmed a non-empty manifest, confirmed `audio/wav` in `Builder::supported_mime_types()`, and confirmed the result remained a normal RIFF/WAVE PCM file.
- The full upstream CTest run was not a valid offline pass: 195 of 369 C++ tests passed, 174 tests attempted public timestamp or remote-manifest URLs and failed because network resolution was unavailable, and the separate C test executable had not been built. These failures do not contradict the isolated offline WAV result, but CI must split offline tests from explicit network tests.

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
