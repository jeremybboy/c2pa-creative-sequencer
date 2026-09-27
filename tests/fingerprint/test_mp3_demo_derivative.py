import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).parents[2] / "scripts/make_mp3_demo_derivative.py"
SPEC = importlib.util.spec_from_file_location("mp3_demo_derivative", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class Mp3DemoDerivativeTests(unittest.TestCase):
    def test_uses_measured_fallback_transformation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.wav"
            output = root / "derivative.mp3"
            source.write_bytes(b"fixture")

            with mock.patch.object(MODULE.subprocess, "run") as run:
                command = MODULE.create_derivative(source, output, "/usr/bin/ffmpeg")

            run.assert_called_once_with(command, check=True)
            self.assertIn("-map_metadata", command)
            self.assertEqual(command[command.index("-af") + 1], "lowpass=f=12000")
            self.assertEqual(command[command.index("-b:a") + 1], "64k")
            self.assertEqual(command[-1], str(output))


if __name__ == "__main__":
    unittest.main()
