"""Run with: python3 tools/tests/test_ucxt_output.py /path/to/ucxt"""
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

UCXT = str(pathlib.Path(sys.argv.pop(1)).resolve())
ROOT = pathlib.Path(__file__).resolve().parents[2]


class UcxtOutput(unittest.TestCase):
    def test_redirected_output_matches_stdout(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = pathlib.Path(directory)
            for name in ('u7misc.data', 'u7opcodes.data'):
                shutil.copyfile(ROOT / 'tools/ucxt/data' / name, directory / name)
            shutil.copyfile(ROOT / 'data/u7bgintrinsics.data',
                            directory / 'u7bgintrinsics.data')
            # Function 0x900: no strings, arguments, locals or externs; just ret.
            source = directory / 'input.uco'
            source.write_bytes(struct.pack('<6H', 0x900, 9, 0, 0, 0, 0) + b'\x25')
            for output_format in ('-fa', '-fs'):
                with self.subTest(output_format=output_format):
                    command = [UCXT, '-nc', '-bg', '-i' + str(source), '-a', output_format]
                    expected = subprocess.run(command, cwd=directory, capture_output=True,
                                              timeout=10)
                    self.assertEqual(expected.returncode, 0, expected.stderr)
                    self.assertEqual(expected.stderr.replace(b'\r\n', b'\n'), b'Functions: 1\n')
                    self.assertIn(b'0900', expected.stdout)
                    output = directory / 'output.txt'
                    actual = subprocess.run(command + ['-o' + str(output)], cwd=directory,
                                            capture_output=True, timeout=10)
                    self.assertEqual(actual.returncode, 0, actual.stderr)
                    self.assertEqual(actual.stderr, expected.stderr)
                    self.assertEqual(actual.stdout, b'')
                    self.assertEqual(output.read_bytes().replace(b'\r\n', b'\n'),
                                     expected.stdout.replace(b'\r\n', b'\n'))


if __name__ == '__main__':
    unittest.main()
