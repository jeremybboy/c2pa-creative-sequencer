#!/usr/bin/env python3
import http.client
import hashlib
import json
import pathlib
import struct
import sys
import tempfile
import threading
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from constants import ALGORITHM, FINGERPRINT_ALGORITHM
from repository import Repository
from server import make_server
from service import ResolverService


PAYLOAD = "0123456789abcdef0011223344556677"
MANIFEST_ID = "urn:uuid:resolver-test"
MANIFEST_BYTES = b"exact-c2pa-manifest-store\x00\xff"


def wave_bytes(with_c2pa=False):
    chunks = [b"fmt " + struct.pack("<IHHIIHH", 16, 1, 2, 48000, 192000, 4, 16)]
    if with_c2pa:
        chunks.append(b"c2pa" + struct.pack("<I", 4) + b"test")
    chunks.append(b"data" + struct.pack("<I", 4) + b"\x00\x00\x00\x00")
    body = b"WAVE" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(body)) + body


class FakeDecoder:
    def __init__(self, candidates=None):
        self.candidates = candidates or []

    def decode_candidates(self, _path):
        return list(self.candidates)


class FakeFingerprintMatcher:
    def __init__(self, matches=None):
        self.matches = matches or []

    def match_file(self, _path, _repository):
        return {"queryHashCount": 100, "queryDurationSeconds": 2.0,
                "matches": list(self.matches)}


class ResolverTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temporary.name)
        self.repository = Repository(self.root / "repository")
        self.outbox = self.root / "outbox"
        package = self.outbox / PAYLOAD
        package.mkdir(parents=True)
        (package / "manifest.c2pa").write_bytes(MANIFEST_BYTES)
        (package / "binding.json").write_text(json.dumps({
            "algorithm": ALGORITHM,
            "value": PAYLOAD,
            "manifestId": MANIFEST_ID,
            "manifestFile": "manifest.c2pa",
            "title": "resolver-test.wav",
            "claimGenerator": "C2PA Creative Sequencer",
        }))

    def tearDown(self):
        self.temporary.cleanup()

    def write_fingerprint_package(self):
        artifact = b"audfprinthashV00" + struct.pack("<2i", 1, 42)
        value = hashlib.sha256(artifact).hexdigest()
        package = self.outbox / value
        package.mkdir(parents=True)
        (package / "manifest.c2pa").write_bytes(MANIFEST_BYTES)
        (package / "fingerprint-data.afpt").write_bytes(artifact)
        (package / "fingerprint.json").write_text(json.dumps({
            "algorithm": FINGERPRINT_ALGORITHM, "value": value,
            "registrationFile": "fingerprint-data.afpt",
        }))
        (package / "binding.json").write_text(json.dumps({
            "schemaVersion": 2, "publicationId": value,
            "manifestId": MANIFEST_ID, "manifestFile": "manifest.c2pa",
            "title": "resolver-test.wav", "claimGenerator": "C2PA Creative Sequencer",
            "bindings": [{"type": "fingerprint", "algorithm": FINGERPRINT_ALGORITHM,
                          "value": value, "registrationFile": "fingerprint-data.afpt"}],
        }))
        return value

    def test_outbox_import_is_exact_and_idempotent(self):
        first = self.repository.import_outbox(self.outbox)
        second = self.repository.import_outbox(self.outbox)
        self.assertEqual(first["imported"], 1)
        self.assertEqual(second["alreadyImported"], 1)
        self.assertEqual(self.repository.get_manifest(MANIFEST_ID), MANIFEST_BYTES)
        self.assertEqual(len(self.repository.matches(ALGORITHM, PAYLOAD)), 1)

    def test_content_match_requires_exact_repository_membership(self):
        self.repository.import_outbox(self.outbox)
        matched = ResolverService(
            self.repository, FakeDecoder(["f" * 32, PAYLOAD])
        ).match_content_bytes(wave_bytes())
        self.assertEqual(matched["decodedCandidateCount"], 2)
        self.assertEqual(len(matched["matches"]), 1)
        self.assertFalse(matched["embeddedContentCredentials"])
        self.assertIn("has not passed the original asset hard binding",
                      matched["matches"][0]["validationSummary"])
        unmatched = ResolverService(
            self.repository, FakeDecoder(["e" * 32])
        ).match_content_bytes(wave_bytes(with_c2pa=True))
        self.assertEqual(unmatched["matches"], [])
        self.assertTrue(unmatched["embeddedContentCredentials"])

    def test_versioned_fingerprint_import_similarity_and_negative(self):
        value = self.write_fingerprint_package()
        result = self.repository.import_outbox(self.outbox)
        self.assertEqual(result["imported"], 2)
        registration = self.repository.fingerprint_registrations()[0]
        evidence = {"name": registration["registrationId"], "alignedHashes": 37,
                    "rawCommonHashes": 52, "queryHashCount": 100,
                    "queryCoverage": 0.37, "offsetSeconds": 0.0,
                    "matchedSeconds": 1.8, "rank": 0}
        service = ResolverService(self.repository, FakeDecoder(),
                                  FakeFingerprintMatcher([evidence]))
        matched = service.match_fingerprint_content_bytes(wave_bytes())
        self.assertEqual(matched["matches"][0]["value"], value)
        self.assertEqual(matched["matches"][0]["alignedHashes"], 37)
        negative = ResolverService(self.repository, FakeDecoder(),
                                   FakeFingerprintMatcher()).match_fingerprint_content_bytes(
                                       wave_bytes())
        self.assertEqual(negative["matches"], [])
        self.assertEqual(negative["message"],
                         "No matching fingerprint-bound Content Credentials found.")

    def test_sbr_inspired_http_routes(self):
        self.repository.import_outbox(self.outbox)
        server = make_server("127.0.0.1", 0, self.repository,
                             FakeDecoder([PAYLOAD]), self.outbox,
                             fingerprint_matcher=FakeFingerprintMatcher())
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            connection = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=5)
            connection.request("GET", "/services/supportedAlgorithms")
            supported = json.loads(connection.getresponse().read())
            self.assertEqual(supported["algorithms"], [ALGORITHM, FINGERPRINT_ALGORITHM])
            connection.request("GET", "/")
            landing = connection.getresponse().read().decode()
            self.assertIn("Watermark Recovery", landing)
            self.assertIn("Fingerprint Recovery", landing)
            connection.request("GET", "/watermark")
            self.assertIn("EXACT LOOKUP", connection.getresponse().read().decode())
            connection.request("GET", "/fingerprint")
            fingerprint_page = connection.getresponse().read().decode()
            self.assertIn("SIMILARITY MATCH", fingerprint_page)
            self.assertIn("Similarity evidence", fingerprint_page)
            connection.request(
                "GET", "/matches/byBinding?algorithm=" + ALGORITHM + "&value=" + PAYLOAD
            )
            binding = json.loads(connection.getresponse().read())
            self.assertEqual(binding["matches"][0]["manifestId"], MANIFEST_ID)
            connection.request("POST", "/matches/byContent", wave_bytes(), {
                "Content-Type": "audio/wav", "X-Filename": "test.wav"
            })
            content = json.loads(connection.getresponse().read())
            self.assertEqual(content["matches"][0]["value"], PAYLOAD)
            connection.request("GET", "/manifests/" + MANIFEST_ID)
            response = connection.getresponse()
            self.assertEqual(response.status, 200)
            self.assertEqual(response.getheader("Content-Type"), "application/c2pa")
            self.assertEqual(
                response.getheader("Content-Disposition"),
                'attachment; filename="recovered-manifest.c2pa"'
            )
            self.assertEqual(response.read(), MANIFEST_BYTES)
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=5)


if __name__ == "__main__":
    unittest.main()
