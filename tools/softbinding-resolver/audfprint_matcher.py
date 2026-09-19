import hashlib
import json
import os
import pathlib
import subprocess
import tempfile
import threading

from constants import FINGERPRINT_MIN_ALIGNED_HASHES


def default_executable():
    override = os.environ.get("C2PASEQ_AUDFPRINT_EXECUTABLE")
    if override:
        return pathlib.Path(override)
    return pathlib.Path.home() / (
        "Library/Application Support/C2PA Creative Sequencer/"
        "Fingerprint/bin/c2paseq-audfprint"
    )


class AudfprintMatcher:
    def __init__(self, executable=None):
        self.executable = pathlib.Path(executable or default_executable())
        self.lock = threading.RLock()

    def _run(self, arguments):
        if not self.executable.is_file():
            raise ValueError("audfprint runtime unavailable; run scripts/setup_audfprint.sh")
        completed = subprocess.run(
            [str(self.executable), *arguments], check=True, capture_output=True, text=True
        )
        lines = [line for line in completed.stdout.splitlines() if line.strip().startswith("{")]
        if not lines:
            raise ValueError("audfprint helper returned no JSON result")
        return json.loads(lines[-1])

    def _database_signature(self, registrations):
        material = "\n".join(
            f"{item['registrationId']}:{item['value']}:{item['path']}"
            for item in sorted(registrations, key=lambda entry: entry["registrationId"])
        )
        return hashlib.sha256(material.encode()).hexdigest()

    def _ensure_database(self, repository):
        registrations = repository.fingerprint_registrations()
        database = repository.root / "audfprint-database.pklz"
        signature_path = repository.root / "audfprint-database.sha256"
        signature = self._database_signature(registrations)
        if database.exists() and signature_path.exists() \
                and signature_path.read_text().strip() == signature:
            return database
        database.unlink(missing_ok=True)
        for item in registrations:
            self._run([
                "register", "--database", str(database),
                "--artifact", str(repository.root / item["path"]),
                "--name", item["registrationId"],
            ])
        signature_path.write_text(signature + "\n")
        return database

    def match_file(self, path, repository):
        with self.lock:
            if not repository.fingerprint_registrations():
                return {"queryHashCount": 0, "queryDurationSeconds": 0.0, "matches": []}
            database = self._ensure_database(repository)
            return self._run([
                "match", "--database", str(database), "--query", str(path),
                "--min-count", str(FINGERPRINT_MIN_ALIGNED_HASHES), "--max-matches", "5",
            ])
