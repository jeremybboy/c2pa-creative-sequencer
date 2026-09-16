import os
import pathlib
import re
import subprocess


PATTERN = re.compile(r"\b[0-9a-fA-F]{32}\b")


def default_executable():
    override = os.environ.get("C2PASEQ_AUDIOWMARK_EXECUTABLE")
    if override:
        return pathlib.Path(override)
    return pathlib.Path.home() / (
        "Library/Application Support/C2PA Creative Sequencer/AudioWMark/bin/audiowmark"
    )


class AudioWMarkDecoder:
    def __init__(self, executable=None, timeout=180):
        self.executable = pathlib.Path(executable or default_executable())
        self.timeout = timeout

    def available(self):
        return self.executable.is_file() and os.access(self.executable, os.X_OK)

    def decode_candidates(self, audio_path):
        if not self.available():
            raise RuntimeError("AudioWMark runtime is unavailable; run scripts/setup_audiowmark.sh")
        completed = subprocess.run(
            [str(self.executable), "get", "--strict", str(audio_path)],
            capture_output=True, text=True, timeout=self.timeout, check=False
        )
        if completed.returncode != 0:
            raise RuntimeError((completed.stderr or completed.stdout).strip())
        candidates = []
        for line in completed.stdout.splitlines():
            if not line.startswith("pattern"):
                continue
            match = PATTERN.search(line)
            if match and match.group(0).lower() not in candidates:
                candidates.append(match.group(0).lower())
        return candidates
