import importlib.util
import pathlib
import struct
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).parents[2] / "scripts/make_softbinding_demo_derivative.py"
SPEC = importlib.util.spec_from_file_location("demo_derivative", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DemoDerivativeTests(unittest.TestCase):
    def test_removes_only_c2pa_chunk(self):
        fmt = b"fmt " + struct.pack("<I", 4) + b"fmt!"
        c2pa = b"c2pa" + struct.pack("<I", 4) + b"test"
        audio = b"data" + struct.pack("<I", 4) + b"pcm!"
        body = b"WAVE" + fmt + c2pa + audio
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "source.wav"
            output = pathlib.Path(directory) / "output.wav"
            source.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
            MODULE.create_derivative(source, output)
            result = output.read_bytes()
            self.assertNotIn(b"c2pa", result)
            self.assertIn(fmt, result)
            self.assertIn(audio, result)


if __name__ == "__main__":
    unittest.main()
