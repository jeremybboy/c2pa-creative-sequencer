#!/usr/bin/env python3
"""Opt-in real WAV -> MP3 fingerprint recovery and independent watermark check."""

import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
REPOSITORY_ROOT = ROOT.parents[1]
sys.path.insert(0, str(ROOT))

from audfprint_matcher import AudfprintMatcher
from audiowmark_decoder import AudioWMarkDecoder
from repository import Repository
from service import ResolverService


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--outbox", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--mp3", required=True)
    parser.add_argument("--unrelated", required=True)
    parser.add_argument("--manifest-id", required=True)
    parser.add_argument("--watermark-value", required=True)
    args = parser.parse_args()
    source, mp3 = pathlib.Path(args.source), pathlib.Path(args.mp3)
    subprocess.run([sys.executable, str(REPOSITORY_ROOT / "scripts/make_mp3_demo_derivative.py"),
                    str(source), str(mp3)], check=True)
    repository = Repository(args.repository)
    first, second = repository.import_outbox(args.outbox), repository.import_outbox(args.outbox)
    if first["imported"] != 1 or second["alreadyImported"] != 1 or first["errors"]:
        raise SystemExit(f"publication import failed or was not idempotent: {first} / {second}")
    service = ResolverService(repository, AudioWMarkDecoder(), AudfprintMatcher())
    fingerprint = service.match_fingerprint_content_bytes(mp3.read_bytes(), ".mp3")
    correct = [item for item in fingerprint["matches"]
               if item["manifestId"] == args.manifest_id]
    if not correct:
        raise SystemExit(f"MP3 fingerprint did not recover expected manifest: {fingerprint}")
    negative = service.match_fingerprint_content_bytes(
        pathlib.Path(args.unrelated).read_bytes(), pathlib.Path(args.unrelated).suffix)
    if negative["matches"]:
        raise SystemExit(f"unrelated audio produced false recovery: {negative}")
    try:
        watermark = service.match_watermark_content_bytes(mp3.read_bytes(), ".mp3")
        watermark_match = any(item["value"] == args.watermark_value
                              and item["manifestId"] == args.manifest_id
                              for item in watermark["matches"])
        watermark_evidence = {"decodedCandidateCount": watermark["decodedCandidateCount"],
                              "matched": watermark_match}
    except Exception as error:  # The measured failure is still a valid independent result.
        watermark_evidence = {"matched": False, "error": str(error)}
    package = next(pathlib.Path(args.outbox).iterdir())
    exact_manifest = repository.get_manifest(args.manifest_id) == (package / "manifest.c2pa").read_bytes()
    if not exact_manifest:
        raise SystemExit("resolver did not return exact published manifest bytes")
    print(json.dumps({"mp3": str(mp3), "fingerprint": correct[0],
                      "watermark": watermark_evidence,
                      "negativeMatches": len(negative["matches"]),
                      "exactManifest": exact_manifest}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
