#!/usr/bin/env python3
"""Create the PR 012 strong 64 kbps MP3 derivative with ffmpeg."""

import argparse
import pathlib
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise SystemExit("ffmpeg is required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
        "-i", str(args.input), "-map_metadata", "-1", "-c:a", "libmp3lame",
        "-b:a", "64k", str(args.output),
    ], check=True)
    print(f"Source WAV: {args.input}")
    print(f"MP3 derivative (libmp3lame 64 kbps): {args.output}")


if __name__ == "__main__":
    main()
