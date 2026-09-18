import pathlib
import struct
import tempfile

from constants import ALGORITHM, FINGERPRINT_ALGORITHM, FINGERPRINT_MIN_ALIGNED_HASHES


def has_embedded_c2pa(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return False
    offset = 12
    while offset + 8 <= len(data):
        chunk_id = data[offset:offset + 4].lower()
        size = struct.unpack_from("<I", data, offset + 4)[0]
        end = offset + 8 + size + (size & 1)
        if end > len(data):
            return False
        if chunk_id == b"c2pa":
            return True
        offset = end
    return False


class ResolverService:
    def __init__(self, repository, decoder, fingerprint_matcher=None):
        self.repository = repository
        self.decoder = decoder
        self.fingerprint_matcher = fingerprint_matcher

    def match_content_bytes(self, audio_bytes, suffix=".wav"):
        return self.match_watermark_content_bytes(audio_bytes, suffix)

    def match_watermark_content_bytes(self, audio_bytes, suffix=".wav"):
        with tempfile.NamedTemporaryFile(suffix=suffix, delete=True) as temporary:
            temporary.write(audio_bytes)
            temporary.flush()
            embedded = has_embedded_c2pa(temporary.name)
            candidates = self.decoder.decode_candidates(temporary.name)
        matches = []
        seen = set()
        for value in candidates:
            for item in self.repository.matches(ALGORITHM, value):
                key = (item["manifestId"], value)
                if key in seen:
                    continue
                seen.add(key)
                metadata = item.get("metadata", {})
                matches.append({
                    "manifestId": item["manifestId"],
                    "algorithm": ALGORITHM,
                    "value": value,
                    "title": metadata.get("title", ""),
                    "claimGenerator": metadata.get("claimGenerator", ""),
                    "createdAt": metadata.get("createdAt", ""),
                    "validationSummary": (
                        "Recovered signed manifest; current derivative has not passed "
                        "the original asset hard binding"
                    ),
                })
        return {
            "method": "watermark",
            "embeddedContentCredentials": embedded,
            "watermarkSearched": True,
            "decodedCandidateCount": len(candidates),
            "matches": matches,
            "message": (
                "Content Credentials recovered via audio watermark"
                if matches else "No matching soft-bound Content Credentials found"
            ),
        }

    def match_fingerprint_content_bytes(self, audio_bytes, suffix=".wav"):
        if self.fingerprint_matcher is None:
            raise ValueError("audfprint matcher is not configured")
        with tempfile.NamedTemporaryFile(suffix=suffix, delete=True) as temporary:
            temporary.write(audio_bytes)
            temporary.flush()
            embedded = has_embedded_c2pa(temporary.name) if suffix.lower() == ".wav" else False
            evidence = self.fingerprint_matcher.match_file(temporary.name, self.repository)
        matches = []
        seen = set()
        for candidate in evidence.get("matches", []):
            registration = self.repository.fingerprint_registration(candidate.get("name", ""))
            if registration is None or candidate.get("alignedHashes", 0) < FINGERPRINT_MIN_ALIGNED_HASHES:
                continue
            identity = (registration["manifestId"], registration["value"])
            if identity in seen:
                continue
            seen.add(identity)
            metadata = registration.get("metadata", {})
            matches.append({
                "manifestId": registration["manifestId"],
                "algorithm": FINGERPRINT_ALGORITHM,
                "value": registration["value"],
                "title": metadata.get("title", ""),
                "claimGenerator": metadata.get("claimGenerator", ""),
                "createdAt": metadata.get("createdAt", ""),
                "alignedHashes": candidate["alignedHashes"],
                "rawCommonHashes": candidate["rawCommonHashes"],
                "queryHashCount": candidate["queryHashCount"],
                "queryCoverage": candidate["queryCoverage"],
                "offsetSeconds": candidate["offsetSeconds"],
                "matchedSeconds": candidate["matchedSeconds"],
                "threshold": FINGERPRINT_MIN_ALIGNED_HASHES,
                "validationSummary": (
                    "Recovered signed manifest by perceptual similarity; current derivative "
                    "has not passed the original asset hard binding"
                ),
            })
        return {
            "method": "fingerprint",
            "embeddedContentCredentials": embedded,
            "queryHashCount": evidence.get("queryHashCount", 0),
            "queryDurationSeconds": evidence.get("queryDurationSeconds", 0.0),
            "threshold": FINGERPRINT_MIN_ALIGNED_HASHES,
            "matches": matches,
            "message": (
                "Content Credentials recovered via audio fingerprint"
                if matches else "No matching fingerprint-bound Content Credentials found."
            ),
        }
