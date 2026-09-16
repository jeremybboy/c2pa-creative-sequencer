#!/usr/bin/env python3
"""Offline WavMark adapter for stereo/native-rate C2PA Sequencer WAVs."""

import argparse
import json
import math
import sys

import numpy as np
import resampy
import soundfile as sf
import torch
import wavmark

WAVMARK_RATE = 16000


def payload_bits(payload_hex: str) -> np.ndarray:
    raw = bytes.fromhex(payload_hex)
    if len(raw) != 2:
        raise ValueError("payload must contain exactly 16 bits")
    return np.unpackbits(np.frombuffer(raw, dtype=np.uint8))


def bits_hex(bits: np.ndarray) -> str:
    if bits is None or len(bits) != 16:
        raise ValueError("WavMark decoded no 16-bit payload")
    return np.packbits(np.asarray(bits, dtype=np.uint8)).tobytes().hex().upper()


def mono_16k(audio: np.ndarray, sample_rate: int) -> np.ndarray:
    mono = audio if audio.ndim == 1 else np.mean(audio, axis=1)
    if sample_rate != WAVMARK_RATE:
        mono = resampy.resample(mono, sample_rate, WAVMARK_RATE)
    return np.asarray(mono, dtype=np.float32)


def fit_length(signal: np.ndarray, frames: int) -> np.ndarray:
    if len(signal) >= frames:
        return signal[:frames]
    return np.pad(signal, (0, frames - len(signal)))


def load_model(path: str):
    return wavmark.load_model(path).to(torch.device("cpu"))


def decode(model, audio: np.ndarray, sample_rate: int) -> str:
    decoded, _ = wavmark.decode_watermark(model, mono_16k(audio, sample_rate))
    return bits_hex(decoded)


def embed(args) -> dict:
    original, sample_rate = sf.read(args.input, always_2d=True, dtype="float32")
    if original.shape[1] != 2:
        raise ValueError("soft-binding export requires a stereo render")
    model = load_model(args.model)
    host = mono_16k(original, sample_rate)
    marked, _ = wavmark.encode_watermark(model, host, payload_bits(args.payload))
    residual_16k = marked - host
    residual = residual_16k if sample_rate == WAVMARK_RATE else resampy.resample(
        residual_16k, WAVMARK_RATE, sample_rate
    )
    residual = fit_length(np.asarray(residual, dtype=np.float32), len(original))

    # Apply one coherent residual to both channels. Reduce only the residual if
    # necessary to avoid clipping; exact decode below is still mandatory.
    scale = 1.0
    positive = residual > 0
    negative = residual < 0
    if np.any(positive):
        scale = min(scale, float(np.min((0.999 - original[positive]) / residual[positive, None])))
    if np.any(negative):
        scale = min(scale, float(np.min((-0.999 - original[negative]) / residual[negative, None])))
    scale = max(0.0, min(1.0, scale))
    applied = residual * scale
    output = original + applied[:, None]
    sf.write(args.output, output, sample_rate, subtype="PCM_24", format="WAV")

    reopened, reopened_rate = sf.read(args.output, always_2d=True, dtype="float32")
    decoded_hex = decode(model, reopened, reopened_rate)
    requested = args.payload.upper()
    if decoded_hex != requested:
        raise ValueError(f"post-write WavMark verification mismatch: {decoded_hex} != {requested}")
    noise_rms = math.sqrt(float(np.mean(np.square(reopened - original))))
    signal_rms = math.sqrt(float(np.mean(np.square(original))))
    snr = float("inf") if noise_rms == 0 else 20.0 * math.log10(signal_rms / noise_rms)
    return {
        "ok": True,
        "payload_hex": decoded_hex,
        "sample_rate": reopened_rate,
        "channels": reopened.shape[1],
        "frames": reopened.shape[0],
        "snr_db": snr,
        "residual_scale": scale,
    }


def decode_command(args) -> dict:
    audio, sample_rate = sf.read(args.input, always_2d=True, dtype="float32")
    return {"ok": True, "payload_hex": decode(load_model(args.model), audio, sample_rate)}


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("embed", "decode"):
        command = sub.add_parser(name)
        command.add_argument("--model", required=True)
        command.add_argument("--input", required=True)
        if name == "embed":
            command.add_argument("--output", required=True)
            command.add_argument("--payload", required=True)
    args = parser.parse_args()
    try:
        result = embed(args) if args.command == "embed" else decode_command(args)
        print(json.dumps(result, allow_nan=False))
        return 0
    except Exception as error:  # The C++ boundary needs one explicit failure channel.
        print(json.dumps({"ok": False, "error": str(error)}))
        return 1


if __name__ == "__main__":
    sys.exit(main())
