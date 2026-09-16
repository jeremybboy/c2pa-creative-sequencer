#!/usr/bin/env python3
import pathlib
import struct
import subprocess
import sys
import tempfile


def chunk(name, payload):
    return name + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) & 1 else b"")


with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    source = root / "signed.wav"
    derivative = root / "derivative.wav"
    fmt = chunk(b"fmt ", b"format")
    audio = chunk(b"data", b"PCM-WATERMARK-BYTES")
    c2pa = chunk(b"C2PA", b"manifest-store")
    body = b"WAVE" + fmt + c2pa + audio
    source.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
    subprocess.run([sys.executable, sys.argv[1], str(source), str(derivative)], check=True)
    result = derivative.read_bytes()
    if b"C2PA" in result or audio not in result or fmt not in result:
        raise SystemExit("derivative rewrite did not remove only C2PA")
