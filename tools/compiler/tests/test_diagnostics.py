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

    def test_parser_stack_growth(self):
        source = 'void example 0x900 () { var value = ' + '(' * 300 + '1' + ')' * 300 + '; }\n'
        result = self.compile(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(self.output.exists())

    def test_include_excerpt_and_restore(self):
        include = self.directory / "includes"
        include.mkdir()
        (include / "helper.uc").write_text("void helper 0x901 () { missing = 1; }\n", encoding="ascii")
        result = self.compile('#include "helper.uc"\nvoid example 0x900 () {\n var value = ;\n}\n', "-I", str(include))
        self.assert_failed(result, "helper.uc:1:24: error: use of undeclared identifier 'missing'")
        self.assertIn("  void helper 0x901 () { missing = 1; }\n", result.stdout)
        self.assertIn("input.uc:3:14: error:", result.stdout)

    def test_line_directive(self):
        result = self.compile('#line 42 "generated.uc"\nvoid example 0x900 () { var value = ; }\n')
        self.assert_failed(result, "generated.uc:42:37: error:")

    def test_recover_at_semicolon(self):
        result = self.compile("void example 0x900 () {\n var a = ;\n var b = ;\n missing = 1;\n}\n")
        self.assert_failed(result, "input.uc:2:10: error:")
        self.assertIn("input.uc:3:10: error:", result.stdout)
        self.assertIn("use of undeclared identifier 'missing'", result.stdout)
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
                self.assertNotIn("undeclared identifier 'repeat'", result.stdout)

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
        self.assertNotIn("undeclared identifier 'outer'", result.stdout)

    def test_identifier_location(self):
        result = self.compile('#game "blackgate"\nvoid example 0x900 () {\n missing = 1;\n}\n')
        self.assert_failed(result, "input.uc:3:2: error: use of undeclared identifier 'missing'")
        self.assertIn("   missing = 1;\n   ^~~~~~~\n", result.stdout)

    def test_previous_declaration_note(self):
        result = self.compile("void example 0x900 () {\n var value;\n var value;\n}\n")
        self.assert_failed(result, "input.uc:3:6: error: redefinition of 'value'")
        self.assertIn("input.uc:2:6: note: previous declaration is here", result.stdout)

    def test_semantic_errors_recover_without_crashing(self):
        for invalid in (
            "var duplicate = 1; var duplicate = 2;",
            "struct<Record> duplicate = 1; struct<Record> duplicate = 2;",
            "class<Widget> duplicate = new Widget(); class<Widget> duplicate = new Widget();",
            "var value = -new Widget();",
            "class<Widget> value = unknown();",
            "var value = new Widget()->Unknown::method();",
            "(@65535)();",
            "item->(@65535)();",
            "struct<Unknown>;",
        ):
            with self.subTest(invalid=invalid):
                source = ('#game "blackgate"\nclass Widget {} struct Record { var field; }\n'
                          + 'void example 0x900 () {\n ' + invalid
                          + '\n var following = ;\n}\n')
                result = self.compile(source)
                self.assert_failed(result, "error:")
                self.assertIn("input.uc:5:", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_indirect_call_has_untyped_result(self):
        result = self.compile('#game "blackgate"\nvoid example 0x900 (callback) { var value = (*callback)(); }\n')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_global_declaration_recovery(self):
        for invalid in (
            "const long broken = ;", "enum Broken { value = };",
            "extern var broken (;", "void broken (;", "void broken ( { }",
            "static class<Unknown> broken;", "@ invalid tokens;",
        ):
            with self.subTest(invalid=invalid):
                result = self.compile('#game "blackgate"\n' + invalid
                                      + '\nvoid following 0x900 () { var value = ; }\n')
                self.assert_failed(result, "error:")
                self.assertIn("input.uc:3:", result.stdout)
                self.assertEqual(result.returncode, 1, result.stdout)

    def test_member_declaration_recovery(self):
        source = '''#game "blackgate"
class Widget {
 var broken = ;
 void method(;
 var following;
 void valid() { var broken = ; }
}
struct Record {
 var ;
 @ invalid tokens;
 var field;
}
void following 0x900 () { var broken = ; }
'''
        result = self.compile(source)
        self.assert_failed(result, "error:")
        for line in (3, 4, 6, 9, 10, 13):
            self.assertIn("input.uc:%d:" % line, result.stdout)
        self.assertEqual(result.returncode, 1, result.stdout)

    def test_member_closing_brace_recovery(self):
        result = self.compile('class Widget { var broken = }\nstruct Record { var }\n'
                              + 'void following 0x900 () { var broken = ; }\n')
        self.assert_failed(result, "error:")
        self.assertIn("input.uc:2:", result.stdout)
        self.assertIn("input.uc:3:", result.stdout)

    def test_missing_declaration_semicolon_preserves_next_declaration(self):
        for declaration in ("const long value = 1", "extern void helper 0x901 ()", "static var value"):
            with self.subTest(declaration=declaration):
                result = self.compile(declaration + '\nvoid following 0x900 () { var broken = ; }\n')
                self.assert_failed(result, "error:")
                self.assertEqual(result.stdout.count("error:"), 2, result.stdout)

    def test_missing_local_semicolon_recovers_at_next_delimiter(self):
        result = self.compile("void example 0x900 () {\n var value = 1\n var following = ;\n}\n"
                              + "void following 0x901 () { var broken = ; }\n")
        self.assert_failed(result, "error:")
        self.assertEqual(result.stdout.count("error:"), 2, result.stdout)
        self.assertIn("input.uc:5:", result.stdout)

    def test_codegen_location_and_no_output(self):
        result = self.compile('void helper 0x900 () {}\nvoid example 0x901 () {\n var value = helper();\n}\n')
        self.assert_failed(result, "input.uc:3:14: error: Function 'helper' does not have a return value")

    def test_preserve_previous_output_on_codegen_error(self):
        self.output.write_bytes(b"previous successful build")
        result = self.compile("void example 0x900 () { goto missing; }\n")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Undeclared label", result.stdout)
        self.assertEqual(self.output.read_bytes(), b"previous successful build")

    def test_untruncated_diagnostic(self):
        name = "unknown_" + "x" * 3000
        result = self.compile("void example 0x900 () { " + name + " = 1; }\n")
        self.assert_failed(result, "use of undeclared identifier '" + name + "'")

    def test_diagnostics_use_stderr(self):
        result = subprocess.run([UCC, "-c", "never", "-o", str(self.output)],
                                input="void example 0x900 () { var value = ; }\n",
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                universal_newlines=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error:", result.stderr)
        self.assertNotIn("error:", result.stdout)

    def test_goto_warning_control(self):
        source = "void example 0x900 () { goto target; target: return; }\n"
        enabled = self.compile(source)
        self.assertEqual(enabled.returncode, 0, enabled.stdout)
        self.assertIn("input.uc:1:25: warning: use of 'goto' statement [-Wgoto]", enabled.stdout)
        expected = self.output.read_bytes()
        disabled = self.compile(source, "-Wno-goto")
        self.assertEqual(disabled.returncode, 0, disabled.stdout)
        self.assertNotIn("[-Wgoto]", disabled.stdout)
        self.assertEqual(self.output.read_bytes(), expected)

    def test_integer_warning_control_preserves_bytecode(self):
        source = '''#game "blackgate"
void example 0x900 () {
 var value = 70000, coerced = 65535;
 switch (value) { case 70000: break; case 65535: break; }
}
'''
        enabled = self.compile(source)
        self.assertEqual(enabled.returncode, 0, enabled.stdout)
        self.assertEqual(enabled.stdout.count("[-Winteger-coercion]"), 4, enabled.stdout)
        expected = self.output.read_bytes()
        for options in (("-Wno-integer-coercion",), ("-u",)):
            with self.subTest(options=options):
                disabled = self.compile(source, *options)
                self.assertEqual(disabled.returncode, 0, disabled.stdout)
                self.assertNotIn("warning:", disabled.stdout)
                self.assertEqual(self.output.read_bytes(), expected)

    def test_shape_warning_control_keeps_errors(self):
        source = '''#game "blackgate"
void helper 0x900 () {}
void example 0x901 () { var callback = &helper; script item call callback; }
'''
        for options in ((), ("-Wno-shape-to-function",), ("-u",)):
            with self.subTest(options=options):
                result = self.compile(source, *options)
                self.assert_failed(result, "not declared as 'shape#' or 'object#'")

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
