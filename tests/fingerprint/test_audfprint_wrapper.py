#!/usr/bin/env python3
"""Regression coverage for Finder-style audfprint subprocess environments."""

import os
import pathlib
import stat
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "scripts" / "c2paseq-audfprint"


def make_executable(path, contents):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


class AudfprintWrapperTests(unittest.TestCase):
    def test_recorded_ffmpeg_is_available_with_minimal_gui_path(self):
        with tempfile.TemporaryDirectory() as directory:
            runtime = pathlib.Path(directory) / "Fingerprint Runtime"
            ffmpeg = runtime / "external tools" / "ffmpeg"
            make_executable(ffmpeg, "#!/bin/sh\nexit 0\n")
            make_executable(
                runtime / "venv/bin/python",
                "#!/bin/sh\ncommand -v ffmpeg\n",
            )
            (runtime / "lib").mkdir(parents=True)
            (runtime / "source/audfprint").mkdir(parents=True)
            (runtime / "ffmpeg-path").write_text(str(ffmpeg) + "\n", encoding="utf-8")

            environment = {
                "HOME": directory,
                "PATH": "/usr/bin:/bin",
                "C2PASEQ_AUDFPRINT_RUNTIME": str(runtime),
            }
            result = subprocess.run(
                [str(WRAPPER), "fingerprint", "input.wav", "output.afpt"],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), str(ffmpeg))

    def test_invalid_recorded_ffmpeg_fails_before_python(self):
        with tempfile.TemporaryDirectory() as directory:
            runtime = pathlib.Path(directory) / "Fingerprint"
            runtime.mkdir(parents=True)
            missing = runtime / "missing-ffmpeg"
            (runtime / "ffmpeg-path").write_text(str(missing) + "\n", encoding="utf-8")

            result = subprocess.run(
                [str(WRAPPER), "fingerprint", "input.wav", "output.afpt"],
                check=False,
                capture_output=True,
                text=True,
                env={
                    "HOME": directory,
                    "PATH": "/usr/bin:/bin",
                    "C2PASEQ_AUDFPRINT_RUNTIME": str(runtime),
                },
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ffmpeg executable is unavailable", result.stderr)


if __name__ == "__main__":
    unittest.main()
