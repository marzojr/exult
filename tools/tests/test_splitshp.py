"""Run with: python3 tools/tests/test_splitshp.py /path/to/splitshp /path/to/test_splitshp_readers"""
import pathlib
import subprocess
import sys
import tempfile
import unittest

SPLITSHP = str(pathlib.Path(sys.argv.pop(1)).resolve())
READERS = str(pathlib.Path(sys.argv.pop(1)).resolve())


class ShapeReaders(unittest.TestCase):
    def test_integer_boundaries(self):
        result = subprocess.run([READERS], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_truncated_integer_reads(self):
        for reader, width in (("read1", 1), ("read2", 2), ("read2signed", 2), ("read4", 4)):
            for size in range(width):
                with self.subTest(reader=reader, size=size):
                    result = subprocess.run([READERS, reader], input=b'\0' * size,
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn(b'Unexpected end of shape file', result.stderr)

    def test_split_and_merge_preserve_shapes(self):
        # Two raw tiles, then two opaque RLE frame payloads with shape offsets.
        shapes = (bytes(range(128)),
                  bytes.fromhex('14000000 0c000000 10000000 01020304 05060708'))
        for shape in shapes:
            with self.subTest(shape=shape):
                with tempfile.TemporaryDirectory() as directory:
                    source = pathlib.Path(directory) / 'input.shp'
                    source.write_bytes(shape)
                    result = subprocess.run([SPLITSHP, str(source)], capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    frames = sorted(pathlib.Path(directory).glob('input_*.shp'))
                    self.assertEqual(len(frames), 2)
                    merged = pathlib.Path(directory) / 'merged.shp'
                    result = subprocess.run([SPLITSHP, str(merged), *map(str, frames)],
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(merged.read_bytes(), shape)


if __name__ == '__main__':
    unittest.main()
