# Editor syntax verification — 0.8.40

Run from the release directory:

```sh
python3 tools/editor-syntax-tests-0840/run.py \
  --lvgl /path/to/lvgl-9.6.0 \
  --lvgl-build /path/to/host-lvgl \
  --build /tmp/tab5-editor-syntax-tests
```

The host LVGL directory must contain `liblvgl.a` built with the shipped `lv_conf.h` and compatible AddressSanitizer/UndefinedBehaviorSanitizer options.

The harness compiles the production code and checks:

- Independent language vocabularies, positive and negative C/Python/JSON cases, and each other selectable profile. Types, builtins, decorators, directives, properties, numbers, strings and comments have distinct theme colors. Raw Markdown punctuation and multilingual UTF-8 bytes round-trip unchanged.
- Multiline lexical states, comments/strings split at arbitrary byte offsets, C++ raw literals, Lua long brackets, nested Rust comments, page rendering compared against whole-document rendering and partial identifiers at window boundaries.
- The actual `SyntaxIndex` scans at most 4 KiB per step, respects smaller budgets, invalidates checkpoints after edits, reaches the full 2 MiB document limit and fails gracefully if PSRAM allocation is unavailable. Its storage is bounded to 514 checkpoints per open source document.
- Per-document language settings and view positions survive workspace v2 reload. A legacy v1 fixture migrates to v2; malformed language values and injected write/rename failures are rejected without replacing the last valid workspace.
- The existing .39 codec/save tests run again against the .40 sources, including UTF-8, UTF-16, Windows-1252, transaction rollback and maximum-size documents.
- The actual production `RichView` renders C, Python and JSON with LVGL and the shipped TTF in light/dark themes at 22/24 px. Background context recoloring preserves selection and layout allocation. Screenshots are written into `tools/verification/editor-syntax-0840/`.

The report `tools/verification/editor-syntax-0840.json` records exact production/test/evidence hashes and refuses a passing report if sources changed during the run. Production PSRAM allocations are explicitly counted and balanced. LeakSanitizer is disabled because the host process supervisor prevents its process inspection.

These are host tests, not physical tablet timing or input tests. The actual language dialog call sites are covered by source integration checks and target P4 compilation. The highlighter is lexical: it does not parse embedded JavaScript inside markup, interpolation inside strings, or the full grammar of every supported language. Unknown source extensions remain editable as plain text. The default mode follows the extension; an explicit choice is stored per document, without altering its file content or encoding.
