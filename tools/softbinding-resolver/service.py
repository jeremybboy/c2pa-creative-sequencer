import pathlib
import struct
import tempfile

from constants import ALGORITHM


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
    def __init__(self, repository, decoder):
        self.repository = repository
        self.decoder = decoder

    def match_content_bytes(self, audio_bytes, suffix=".wav"):
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
            "embeddedContentCredentials": embedded,
            "watermarkSearched": True,
            "decodedCandidateCount": len(candidates),
            "matches": matches,
            "message": (
                "Content Credentials recovered via audio watermark"
                if matches else "No matching soft-bound Content Credentials found"
            ),
        }
