# UCC diagnostic regression tests

Build UCC, then run `python3 tools/compiler/tests/test_diagnostics.py /path/to/ucc` from the source tree. Python 3 and the compiler executable are the only requirements; on Windows, ensure UCC's runtime DLL directory is on `PATH`. The tests create isolated temporary inputs and outputs, clear `UCC_INCLUDE`, and place a timeout on each compiler invocation.

The suite checks diagnostic locations and excerpts, include and line-directive handling, color controls, invalid command-line options, failure exit status, statement recovery, restoration of parser and lexer contexts, and warning controls. Warning suppression must preserve bytecode and must not suppress hard errors. Recovery tests include independent errors in following statements and functions to detect premature termination and state leakage.

For compiler changes, also compile `tools/compiler/test1.uc`, BG Keyring's `content/bgkeyring/src/usecode.uc`, and SI Fixes' `content/sifixes/src/usecode.uc`, each with its corresponding include directory and a temporary output path. Compare their diagnostics and bytecode against the baseline for changes that should not affect code generation. `test1.uc` deliberately triggers warnings.

Recovery synchronizes statements, global declarations, and class/struct members at semicolons and closing braces. Recovery restores lexer modes and parser declaration/scope state. A malformed signature can be discarded so later declarations are checked. Missing semicolons after complete declarations can retain the next declaration; incomplete expressions can require discarding through the next delimiter.

Every warning site has a named category. The suite checks category suppression against unchanged bytecode and verifies promotion with `-Werror=NAME`. `-Werror`, `-Wno-error=NAME`, `-Wall`, `-Wno-all`, and `-u` share the same warning policy. See `docs/ucc.1` for the complete category list and command-line options.
