#!/usr/bin/env python3
"""Small JSON adapter around the pinned external dpwe/audfprint runtime."""

import argparse
import json
import pathlib
import sys


def configure_import(source):
    sys.path.insert(0, str(pathlib.Path(source).resolve()))
    import audfprint_analyze  # pylint: disable=import-outside-toplevel
    import audfprint_match  # pylint: disable=import-outside-toplevel
    import hash_table  # pylint: disable=import-outside-toplevel
    return audfprint_analyze, audfprint_match, hash_table


def analyzer(module):
    result = module.Analyzer()
    result.density = 20.0
    result.target_sr = 11025
    result.n_fft = 512
    result.n_hop = 256
    result.shifts = 1
    return result


def fingerprint(args, modules):
    analyze, _, _ = modules
    worker = analyzer(analyze)
    hashes = worker.wavfile2hashes(args.input)
    if len(hashes) == 0:
        raise ValueError("audfprint produced no landmark hashes")
    pathlib.Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    analyze.hashes_save(args.output, hashes)
    return {
        "engineVersion": args.commit,
        "hashCount": len(hashes),
        "durationSeconds": worker.soundfiledur,
    }


def register(args, modules):
    analyze, _, hash_table = modules
    database = pathlib.Path(args.database)
    table = hash_table.HashTable(str(database)) if database.exists() else hash_table.HashTable()
    table.params["samplerate"] = 11025
    hashes = analyze.hashes_load(args.artifact)
    existing = args.name in table.names
    if not existing:
        table.store(args.name, hashes)
        database.parent.mkdir(parents=True, exist_ok=True)
        table.save(str(database))
    return {"registered": not existing, "hashCount": len(hashes), "name": args.name}


def match(args, modules):
    analyze, match_module, hash_table = modules
    database = pathlib.Path(args.database)
    if not database.exists():
        return {"queryHashCount": 0, "queryDurationSeconds": 0.0, "matches": []}
    table = hash_table.HashTable(str(database))
    worker = analyzer(analyze)
    matcher = match_module.Matcher()
    matcher.threshcount = args.min_count
    matcher.exact_count = True
    matcher.find_time_range = True
    matcher.max_returns = args.max_matches
    results, duration, query_hashes = matcher.match_file(worker, table, args.query)
    hop_seconds = worker.n_hop / worker.target_sr
    matches = []
    for row in results:
        track_id, aligned, offset, raw, rank, minimum, maximum = [int(value) for value in row]
        matches.append({
            "name": table.names[track_id],
            "alignedHashes": aligned,
            "rawCommonHashes": raw,
            "queryHashCount": query_hashes,
            "queryCoverage": aligned / max(1, query_hashes),
            "offsetSeconds": offset * hop_seconds,
            "matchedSeconds": max(0, maximum - minimum) * hop_seconds,
            "rank": rank,
        })
    return {"queryHashCount": query_hashes, "queryDurationSeconds": duration, "matches": matches}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--commit", required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    make = commands.add_parser("fingerprint")
    make.add_argument("input")
    make.add_argument("output")
    add = commands.add_parser("register")
    add.add_argument("--database", required=True)
    add.add_argument("--artifact", required=True)
    add.add_argument("--name", required=True)
    query = commands.add_parser("match")
    query.add_argument("--database", required=True)
    query.add_argument("--query", required=True)
    query.add_argument("--min-count", type=int, default=10)
    query.add_argument("--max-matches", type=int, default=5)
    args = parser.parse_args()
    modules = configure_import(args.source)
    operations = {"fingerprint": fingerprint, "register": register, "match": match}
    print(json.dumps(operations[args.command](args, modules), separators=(",", ":")))


if __name__ == "__main__":
    main()
