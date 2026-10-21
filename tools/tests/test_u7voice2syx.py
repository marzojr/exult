"""Run with: python3 tools/tests/test_u7voice2syx.py /path/to/u7voice2syx"""
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

CONVERTER = str(pathlib.Path(sys.argv.pop(1)).resolve())


def timbre_flex(patches):
    data = bytes([len(patches)]) + b''.join(bytes(246) + bytes([patch]) for patch in patches)
    header = bytearray(128)
    struct.pack_into('<3I', header, 80, 0xffff1a00, 1, 0xcc)
    return header + struct.pack('<2I', 136, len(data)) + data


class TimbreConversion(unittest.TestCase):
    def test_patch_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            (directory / 'u7voice.flx').write_bytes(timbre_flex([1, 128]))
            result = subprocess.run([CONVERTER], cwd=directory, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            output = (directory / 'u7voice.syx').read_bytes()
            messages = [message for message in output.split(b'\xf7') if message]
            patches = [message for message in messages if message[5] == 5]
            self.assertEqual(len(patches), 2)
            self.assertEqual(patches[0][5:8], bytes([5, 0, 0]))
            self.assertEqual(patches[1][5:8], bytes([5, 7, 120]))
            for index, message in enumerate(patches):
                self.assertEqual(message[8:16], bytes([2, index, 24, 50, 24, 0, 1, 0]))

    def test_invalid_patch_numbers(self):
        for patch in (0, 129, 255):
            with self.subTest(patch=patch), tempfile.TemporaryDirectory() as directory:
                directory = pathlib.Path(directory)
                (directory / 'u7voice.flx').write_bytes(timbre_flex([patch]))
                result = subprocess.run([CONVERTER], cwd=directory, capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(b'Patch number must be between 1 and 128', result.stderr)
                output = (directory / 'u7voice.syx').read_bytes()
                self.assertNotIn(b'\xf0\x41\x10\x16\x12\x05', output)


if __name__ == '__main__':
    unittest.main()
