"""Create reproducible, local MP3 derivatives for the recovery demonstration."""

import os
import pathlib
import shutil
import subprocess
import tempfile


PRESETS = {
    "mp3-320k": {
        "label": "MP3 320 kbps",
        "description": "Light lossy encode; metadata stripped",
        "bitrate": "320k",
        "filters": [],
    },
    "mp3-64k": {
        "label": "MP3 64 kbps",
        "description": "Stronger lossy encode; metadata stripped",
        "bitrate": "64k",
        "filters": [],
    },
    "mp3-64k-lowpass-12k": {
        "label": "MP3 64 kbps + 12 kHz low-pass",
        "description": "PR 013 fallback candidate; metadata stripped",
        "bitrate": "64k",
        "filters": ["lowpass=f=12000"],
    },
}


class DerivativeRuntimeUnavailable(RuntimeError):
    """Raised when a usable FFmpeg executable cannot be found."""


def _is_executable(path):
    return path and path.is_file() and os.access(path, os.X_OK)


def default_ffmpeg_record():
    return (pathlib.Path.home() / "Library/Application Support"
            / "C2PA Creative Sequencer/Fingerprint/ffmpeg-path")


def resolve_ffmpeg(explicit=None):
    candidates = []
    if explicit:
        candidates.append(pathlib.Path(explicit).expanduser())
    configured = os.environ.get("C2PASEQ_FFMPEG")
    if configured:
        candidates.append(pathlib.Path(configured).expanduser())
    record = default_ffmpeg_record()
    if record.is_file():
        recorded = record.read_text(encoding="utf-8").splitlines()
        if recorded:
            candidates.append(pathlib.Path(recorded[0]).expanduser())
    discovered = shutil.which("ffmpeg")
    if discovered:
        candidates.append(pathlib.Path(discovered))
    candidates.extend((pathlib.Path("/opt/homebrew/bin/ffmpeg"),
                       pathlib.Path("/usr/local/bin/ffmpeg")))
    for candidate in candidates:
        if _is_executable(candidate):
            return candidate.resolve()
    return None


class DerivativeCreator:
    def __init__(self, ffmpeg=None, runner=None):
        self.ffmpeg = resolve_ffmpeg(ffmpeg)
        self.runner = runner or subprocess.run

    def status(self):
        return {
            "available": self.ffmpeg is not None,
            "presets": [
                {"id": identifier, "label": value["label"],
                 "description": value["description"]}
                for identifier, value in PRESETS.items()
            ],
            "message": ("Ready to create local MP3 derivatives."
                        if self.ffmpeg else
                        "FFmpeg was not found. Run scripts/setup_audfprint.sh "
                        "or install FFmpeg, then relaunch the demo."),
        }

    def create_bytes(self, source, source_suffix, preset_id):
        if self.ffmpeg is None:
            raise DerivativeRuntimeUnavailable(self.status()["message"])
        preset = PRESETS.get(preset_id)
        if preset is None:
            raise ValueError("unknown derivative preset")
        safe_suffix = source_suffix.lower() if source_suffix else ".wav"
        if len(safe_suffix) > 10 or not safe_suffix.startswith("."):
            safe_suffix = ".wav"
        with tempfile.TemporaryDirectory(prefix="c2paseq-derivative-") as directory:
            root = pathlib.Path(directory)
            input_path = root / ("source" + safe_suffix)
            output_path = root / "derivative.mp3"
            input_path.write_bytes(source)
            command = [
                str(self.ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
                "-i", str(input_path), "-map_metadata", "-1", "-vn",
            ]
            if preset["filters"]:
                command.extend(("-af", ",".join(preset["filters"])))
            command.extend(("-c:a", "libmp3lame", "-b:a", preset["bitrate"],
                            str(output_path)))
            self.runner(command, check=True)
            if not output_path.is_file() or output_path.stat().st_size == 0:
                raise RuntimeError("FFmpeg did not create an MP3 derivative")
            return output_path.read_bytes(), preset
