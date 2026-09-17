#!/usr/bin/env python3
"""Remove C2PA RIFF chunks while preserving all other WAV bytes."""

import argparse
import pathlib
import struct


def create_derivative(source, destination):
    data = pathlib.Path(source).read_bytes()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("source is not a RIFF/WAVE file")
    chunks = []
    offset = 12
    removed = 0
    while offset + 8 <= len(data):
        size = struct.unpack_from("<I", data, offset + 4)[0]
        end = offset + 8 + size + (size & 1)
        if end > len(data):
            raise ValueError("source contains a truncated RIFF chunk")
        chunk = data[offset:end]
        if data[offset:offset + 4].lower() == b"c2pa":
            removed += 1
        else:
            chunks.append(chunk)
        offset = end
    if removed == 0:
        raise ValueError("source contains no C2PA RIFF chunk")
    body = b"WAVE" + b"".join(chunks)
    pathlib.Path(destination).write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("destination")
    args = parser.parse_args()
    create_derivative(args.source, args.destination)


if __name__ == "__main__":
    main()
