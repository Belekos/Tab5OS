# Linux / GCC compilation

Follow the [complete procedure](../../../docs/BUILD.md#linux-x86_64) for Tab5OS 0.8.53.

- `prepare_linux.py` prepares the isolated Arduino environment.
- `verify_downloads.py` verifies archives against `dependency-downloads.json`.
- `compile_linux.py` selects the sketch, prepares adaptations and compiles.
- `verify_linux.py` checks the image and linked adaptations. `--baseline` accepts an optional reference; historical reports are not required.

Keep the `prepare_*` and `verify_*` scripts together, along with the LVGL preparation script in the neighbouring `build_clang` directory.

To compile a specific object, generate the database with `compile_linux.py --database-only`, then use `target_compile.py --database <compile_commands.json> --build <directory> --report <report.json> <Source.cpp>`. `main_target.py --environment <directory>` regenerates and compiles the main sketch; results go into `qa` in that environment. These object checks do not replace a full link.
