#!/usr/bin/env python3
"""Run with: python3 tools/compiler/tests/test_diagnostics.py /path/to/ucc"""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


UCC = str(Path(sys.argv.pop(1)).resolve())


class Diagnostics(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ucc-diagnostics-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.output = self.directory / "output.uco"

    def compile(self, source, *options, color="never"):
        path = self.directory / "input.uc"
        path.write_text(source, encoding="ascii")
        return self.run_ucc("-c", color, "-o", str(self.output), *options, str(path))

    def run_ucc(self, *args):
        env = os.environ.copy()
        env.pop("UCC_INCLUDE", None)
        return subprocess.run(
            [UCC, *args], input="", stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True,
            cwd=str(self.directory), env=env, timeout=15,
        )

    def assert_failed(self, result, message):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(message, result.stdout)
        self.assertFalse(self.output.exists(), result.stdout)

    def test_plain_success(self):
        result = self.compile("void example 0x900 () {}\n")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())
        self.assertNotIn("\033[", result.stdout)

    def test_plain_failure(self):
        result = self.compile("void example 0x900 () { var x = ; }\n")
        self.assert_failed(result, "syntax error")
        self.assertNotIn("\033[", result.stdout)

    def test_forced_color(self):
        for source in ("void example 0x900 () {}\n", "void\n"):
            with self.subTest(source=source):
                result = self.compile(source, color="always")
                self.assertIn("\033[", result.stdout)

    def test_unknown_warning(self):
        self.assert_failed(self.compile("void example 0x900 () {}\n", "-Wtypo"),
                           "Invalid argument to -W: 'typo'")

    def test_unknown_option(self):
        self.assert_failed(self.run_ucc("-Q"), "unknown option '-Q'")

    def test_missing_option_argument(self):
        for option in ("-o", "-I", "-c", "-W"):
            with self.subTest(option=option):
                self.assert_failed(self.run_ucc(option), "missing argument to '" + option + "'")

    def test_extra_input(self):
        self.assert_failed(self.run_ucc("first.uc", "second.uc"), "expected a single input file")

    def test_many_errors_still_fail(self):
        # Each distinct undeclared name produces one error.
        source = "void example 0x900 () {\n" + "".join(
            "missing%d = 1;\n" % i for i in range(256)
        ) + "}\n"
        self.assert_failed(self.compile(source), "Usecode compilation failed!")


if __name__ == "__main__":
    unittest.main()
