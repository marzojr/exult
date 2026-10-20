# UCC diagnostic regression tests

Build UCC, then run `python3 tools/compiler/tests/test_diagnostics.py /path/to/ucc` from the source tree. Python 3 and the compiler executable are the only requirements; on Windows, ensure UCC's runtime DLL directory is on `PATH`. The tests create isolated temporary inputs and outputs, clear `UCC_INCLUDE`, and place a timeout on each compiler invocation.

The suite checks diagnostic locations and excerpts, include and line-directive handling, color controls, invalid command-line options, failure exit status, statement recovery, restoration of parser and lexer contexts, and warning controls. Warning suppression must preserve bytecode and must not suppress hard errors. Recovery tests include independent errors in following statements and functions to detect premature termination and state leakage.

For compiler changes, also compile `tools/compiler/test1.uc`, BG Keyring's `content/bgkeyring/src/usecode.uc`, and SI Fixes' `content/sifixes/src/usecode.uc`, each with its corresponding include directory and a temporary output path. Compare their diagnostics and bytecode against the baseline for changes that should not affect code generation. `test1.uc` deliberately triggers warnings.

Recovery currently synchronizes statements at semicolons and block-closing braces. Malformed top-level declarations and function signatures need additional recovery boundaries. Some semantic diagnostics still use the parser's current lexer location; migrate them to saved token or AST locations as they are improved.
