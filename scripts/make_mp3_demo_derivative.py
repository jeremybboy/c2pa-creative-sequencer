#!/usr/bin/env python3
"""Create the PR 013 64 kbps/12 kHz low-pass MP3 derivative with ffmpeg."""

import argparse
import pathlib
import shutil
import subprocess


def create_derivative(input_path, output_path, ffmpeg=None):
    ffmpeg = ffmpeg or shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
        "-i", str(input_path), "-map_metadata", "-1",
        "-af", "lowpass=f=12000", "-c:a", "libmp3lame", "-b:a", "64k",
        str(output_path),
    ]
    subprocess.run(command, check=True)
    return command


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    try:
        create_derivative(args.input, args.output)
    except RuntimeError as error:
        raise SystemExit(str(error)) from error
    print(f"Source WAV: {args.input}")
    print(f"MP3 derivative (libmp3lame 64 kbps, 12 kHz low-pass): {args.output}")


if __name__ == "__main__":
    main()
