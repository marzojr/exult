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

    def test_token_column_and_caret(self):
        result = self.compile("void example 0x900 () {\n    var value = ;\n}\n")
        self.assert_failed(result, "input.uc:2:17: error: syntax error, unexpected ';'")
        self.assertIn("      var value = ;\n                  ^\n", result.stdout)

    def test_tab_expansion(self):
        result = self.compile("void example 0x900 () {\n\tvar value = ;\n}\n")
        self.assert_failed(result, "input.uc:2:14: error:")
        self.assertIn("          var value = ;\n                      ^\n", result.stdout)

    def test_multiline_string_location(self):
        result = self.compile('void example 0x900 () {\n say("first\nsecond");\n var value = ;\n}\n')
        self.assert_failed(result, "input.uc:4:14: error:")

    def test_multiline_comment_location(self):
        result = self.compile("/* comment\n * continued\n */\nvoid example 0x900 () {\n var value = ;\n}\n")
        self.assert_failed(result, "input.uc:5:14: error:")

    def test_eof_location(self):
        result = self.compile("void example 0x900 () {\n")
        self.assert_failed(result, "input.uc:2:1: error: syntax error, unexpected end of file")

    def test_include_excerpt_and_restore(self):
        include = self.directory / "includes"
        include.mkdir()
        (include / "helper.uc").write_text("void helper 0x901 () { missing = 1; }\n", encoding="ascii")
        result = self.compile('#include "helper.uc"\nvoid example 0x900 () {\n var value = ;\n}\n', "-I", str(include))
        self.assert_failed(result, "helper.uc:1:32: error:")
        self.assertIn("  void helper 0x901 () { missing = 1; }\n", result.stdout)
        self.assertIn("input.uc:3:14: error:", result.stdout)

    def test_line_directive(self):
        result = self.compile('#line 42 "generated.uc"\nvoid example 0x900 () { var value = ; }\n')
        self.assert_failed(result, "generated.uc:42:37: error:")

    def test_recover_at_semicolon(self):
        result = self.compile("void example 0x900 () {\n var a = ;\n var b = ;\n missing = 1;\n}\n")
        self.assert_failed(result, "input.uc:2:10: error:")
        self.assertIn("input.uc:3:10: error:", result.stdout)
        self.assertIn("'missing' not declared", result.stdout)
        self.assertEqual(result.stdout.count("error:"), 3, result.stdout)

    def test_recover_at_closing_brace(self):
        result = self.compile("void first 0x900 () { var a = }\nvoid second 0x901 () { var b = ; }\n")
        self.assert_failed(result, "unexpected '}'")
        self.assertIn("input.uc:2:", result.stdout)
        self.assertEqual(result.stdout.count("error:"), 2, result.stdout)

    def test_recovery_contexts(self):
        for malformed in (
            "const long value = ;",  # Constant operand width stack.
            "extern var helper (;",  # Function ID lexer mode and return type.
            "while (true) var value = ;",  # Implicit scope and loop mode.
            "for (value in ;",  # Array loop scope.
            "if (true) var value = ;",  # Implicit scope and nested-if tracking.
            "script item repeat 2 repeat 3 call ;",  # Script mode and repeat nesting.
            "var value = new script call ;",  # Script mode within an expression.
            "switch ( ;",  # Switch scope before its case list starts.
            "class<Widget> broken = ;",  # Class declaration type.
            "struct<Record> broken = ;",  # Struct declaration type.
        ):
            with self.subTest(malformed=malformed):
                result = self.compile(
                    '#game "blackgate"\nclass Widget {} struct Record { var field; } '
                    + 'void example 0x900 () {\n ' + malformed
                    + '\n var repeat = 1;\n break;\n continue;\n'
                    + ' var choice = user_choice;\n var following = ;\n}\n'
                    + 'void following_function 0x901 () { var another = ; }\n'
                )
                self.assert_failed(result, "syntax error")
                self.assertIn("'break' statement not allowed", result.stdout)
                self.assertIn("'continue' statement not allowed", result.stdout)
                self.assertIn("'CHOICE' can only be used", result.stdout)
                self.assertIn("input.uc:8:", result.stdout)
                self.assertIn("input.uc:10:", result.stdout)
                self.assertNotIn("'repeat' not declared", result.stdout)

    def test_recover_inside_scopes(self):
        source = '''#game "blackgate"
void example 0x900 () {
 var outer = 1;
 while (outer) {
  var value = ;
  break;
 }
 try { var value = ; } catch (exception) { var value = ; }
 switch (outer) { case 1: var value = ; break; default: break; }
 converse (0) { case "topic": var value = ; break; }
 outer = 2;
 break;
}
void following_function 0x901 () { var another = ; }
'''
        result = self.compile(source)
        self.assert_failed(result, "syntax error")
        self.assertEqual(result.stdout.count("syntax error"), 6, result.stdout)
        self.assertEqual(result.stdout.count("'break' statement not allowed"), 1, result.stdout)
        self.assertNotIn("'outer' not declared", result.stdout)

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
