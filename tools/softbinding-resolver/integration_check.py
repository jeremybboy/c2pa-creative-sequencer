#!/usr/bin/env python3
import argparse
import pathlib

from audiowmark_decoder import AudioWMarkDecoder
from repository import Repository
from service import ResolverService


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--outbox", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--audio", required=True)
    parser.add_argument("--value", required=True)
    parser.add_argument("--manifest-id", required=True)
    args = parser.parse_args()

    repository = Repository(args.repository)
    first = repository.import_outbox(args.outbox)
    second = repository.import_outbox(args.outbox)
    if first["imported"] != 1 or second["alreadyImported"] != 1:
        raise SystemExit("publication import was not idempotent")
    result = ResolverService(repository, AudioWMarkDecoder()).match_content_bytes(
        pathlib.Path(args.audio).read_bytes(), ".wav"
    )
    matches = [
        match for match in result["matches"]
        if match["value"] == args.value and match["manifestId"] == args.manifest_id
    ]
    if len(matches) != 1:
        raise SystemExit("decoded payload did not resolve to the published manifest")
    package = pathlib.Path(args.outbox) / args.value / "manifest.c2pa"
    if repository.get_manifest(args.manifest_id) != package.read_bytes():
        raise SystemExit("resolver did not return the exact published manifest bytes")


if __name__ == "__main__":
    main()
