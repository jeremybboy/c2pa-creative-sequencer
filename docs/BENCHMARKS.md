# PR 011 AudioWMark Export Benchmarks

Measured on 2026-09-16 on the development Apple-silicon Mac using the Release build, native
AudioWMark 0.6.5, a stereo 24-bit source, and the full Sequencer render/sign/validation path.
Decode is excluded because production export does not decode.

| Audio duration | Render | AudioWMark embed | C2PA sign/validate | Total |
|---:|---:|---:|---:|---:|
| 10 s | 0.733 s | 0.104 s | 0.016 s | 0.860 s |
| 60 s | 0.860 s | 0.123 s | 0.062 s | 1.061 s |
| 180 s | 1.167 s | 0.244 s | 0.156 s | 1.598 s |

These are single-run POC measurements, not broad performance claims. The app runs the complete
pipeline on a worker thread and posts progress/completion to the macOS message thread; manual UI
responsiveness and listening quality still require human acceptance before merge.
