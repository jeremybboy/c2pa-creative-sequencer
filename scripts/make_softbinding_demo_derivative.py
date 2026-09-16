#!/usr/bin/env python3
"""Rewrite PCM WAV chunks while deliberately omitting embedded C2PA/JUMBF."""

import argparse
import struct


def chunks(data: bytes):
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("input is not a RIFF/WAVE file")
    offset = 12
    while offset + 8 <= len(data):
        chunk_id = data[offset:offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        end = offset + 8 + size + (size & 1)
        if end > len(data):
            raise ValueError("malformed WAV chunk")
        yield chunk_id, data[offset:end]
        offset = end


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    source = open(args.input, "rb").read()
    kept = []
    for chunk_id, raw in chunks(source):
        # c2pa-rs embeds WAV manifests in a C2PA RIFF chunk. Preserve audio and
        # ordinary metadata, removing only the credential container.
        if chunk_id.lower() != b"c2pa":
            kept.append(raw)
    body = b"WAVE" + b"".join(kept)
    with open(args.output, "wb") as output:
        output.write(b"RIFF" + struct.pack("<I", len(body)) + body)


if __name__ == "__main__":
    main()
